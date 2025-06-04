#define _POSIX_C_SOURCE 200809L
#define BUILD_CLIENT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

#include "ipc_helpers.h"
#include "markdown.h"

#define FIFO_NAME_MAX 256
#define BUF_SIZE 4096

document *local_doc = NULL;
char *local_log = NULL;
char *permission = NULL;
uint64_t last_logged_version = 0;

pthread_mutex_t local_doc_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t local_log_mutex = PTHREAD_MUTEX_INITIALIZER;

int fd_c2s = -1;
int fd_s2c = -1;

void *pipe_listener_thread(void *arg);
void client_apply_broadcast(const char *msg);

void cleanup_client(void);
void client_handshake(pid_t server_pid, const char *username);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <server_pid> <username>\n", argv[0]);
        return 1;
    }

    pid_t server_pid = (pid_t)atoi(argv[1]);
    const char *username = argv[2];

    client_handshake(server_pid, username);

    pthread_t listener_thread;
    pthread_create(&listener_thread, NULL, pipe_listener_thread, NULL);
    pthread_detach(listener_thread);

    char line[512];
    while (fgets(line, sizeof(line), stdin))
    {
        if (strcmp(line, "DOC?\n") == 0)
        {
            pthread_mutex_lock(&local_doc_mutex);
            markdown_print(local_doc, stdout);
            pthread_mutex_unlock(&local_doc_mutex);
            continue;
        }
        else if (strcmp(line, "LOG?\n") == 0)
        {
            pthread_mutex_lock(&local_log_mutex);
            printf("%s", local_log ? local_log : "");
            pthread_mutex_unlock(&local_log_mutex);
            continue;
        }
        else if (strcmp(line, "PERM?\n") == 0)
        {
            printf("%s", permission ? permission : "(no permission info)\n");
            continue;
        }
        else if (strcmp(line, "DISCONNECT\n") == 0)
        {
            dprintf(fd_c2s, "DISCONNECT\n");
            break;
        }

        // Send to server
        if (write(fd_c2s, line, strlen(line)) < 0)
        {
            perror("Failed to send command to server");
            break;
        }
    }

    cleanup_client();
    return 0;
}

void client_handshake(pid_t server_pid, const char *username)
{
    char fifo_c2s[FIFO_NAME_MAX];
    char fifo_s2c[FIFO_NAME_MAX];
    pid_t client_pid = getpid();

    // Block SIGRTMIN+1
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGRTMIN + 1);
    sigprocmask(SIG_BLOCK, &set, NULL);

    // Send SIGRTMIN to server
    kill(server_pid, SIGRTMIN);

    // Wait for SIGRTMIN+1
    int sig;
    sigwait(&set, &sig);

    // FIFO names
    snprintf(fifo_c2s, sizeof(fifo_c2s), "FIFO_C2S_%d", client_pid);
    snprintf(fifo_s2c, sizeof(fifo_s2c), "FIFO_S2C_%d", client_pid);

    // Open FIFOs
    fd_c2s = open(fifo_c2s, O_WRONLY);
    if (fd_c2s < 0)
    {
        perror("open FIFO_C2S failed");
        exit(1);
    }

    fd_s2c = open(fifo_s2c, O_RDONLY);
    if (fd_s2c < 0)
    {
        perror("open FIFO_S2C failed");
        close(fd_c2s);
        exit(1);
    }

    // Send username
    dprintf(fd_c2s, "%s\n", username);

    // Read role line
    char *role_line = read_line_dynamic(fd_s2c);
    if (!role_line)
    {
        fprintf(stderr, "Failed to read role line\n");
        cleanup_client();
        exit(1);
    }

    if (strcmp(role_line, "Reject UNAUTHORISED") == 0)
    {
        fprintf(stderr, "Server rejected user\n");
        free(role_line);
        cleanup_client();
        exit(1);
    }

    permission = strdup(role_line);
    free(role_line);

    // Read version and ignore
    char *ver_line = read_line_dynamic(fd_s2c);
    if (!ver_line)
    {
        fprintf(stderr, "Failed to read version line\n");
        cleanup_client();
        exit(1);
    }
    free(ver_line);

    // Read document length
    char *len_line = read_line_dynamic(fd_s2c);
    if (!len_line)
    {
        fprintf(stderr, "Failed to read length line\n");
        cleanup_client();
        exit(1);
    }
    size_t doc_len = (size_t)strtoull(len_line, NULL, 10);
    free(len_line);

    // Read document content
    char *buffer = Calloc(doc_len + 1, sizeof(char));
    size_t total_read = 0;
    while (total_read < doc_len)
    {
        ssize_t n = read(fd_s2c, buffer + total_read, doc_len - total_read);
        if (n <= 0)
        {
            fprintf(stderr, "Failed to read document content\n");
            free(buffer);
            cleanup_client();
            exit(1);
        }
        total_read += n;
    }
    buffer[doc_len] = '\0';

    local_doc = markdown_init();
    markdown_parse_string(local_doc, buffer);
    free(buffer);
}

void *pipe_listener_thread(void *arg)
{
    (void)arg;
    char buffer[BUF_SIZE];
    size_t buf_len = 0;

    while (1)
    {
        ssize_t n = read(fd_s2c, buffer + buf_len, BUF_SIZE - buf_len);
        if (n <= 0)
        {
            fprintf(stderr, "Server connection lost.\n");
            exit(1);
        }

        buf_len += n;
        buffer[buf_len] = '\0';

        // Wait until END\n to process broadcast
        char *end_ptr = strstr(buffer, "END\n");
        if (!end_ptr)
            continue;

        size_t full_len = end_ptr - buffer + 4;

        // Extract broadcast string
        char *broadcast = Calloc(full_len + 1, sizeof(char));
        memcpy(broadcast, buffer, full_len);
        broadcast[full_len] = '\0';

        // Get version number from start of broadcast
        uint64_t version = 0;
        sscanf(broadcast, "VERSION %lu", &version);

        // Only append to log if version is new
        if (version > last_logged_version)
        {
            pthread_mutex_lock(&local_log_mutex);
            size_t old_log_len = local_log ? strlen(local_log) : 0;
            local_log = realloc(local_log, old_log_len + full_len + 1);
            memcpy(local_log + old_log_len, broadcast, full_len);
            local_log[old_log_len + full_len] = '\0';
            pthread_mutex_unlock(&local_log_mutex);

            last_logged_version = version;
        }

        pthread_mutex_lock(&local_doc_mutex);
        apply_broadcast(broadcast);
        markdown_increment_version(local_doc);
        pthread_mutex_unlock(&local_doc_mutex);

        // Shift remainder of buffer
        size_t leftover = buf_len - full_len;
        memmove(buffer, buffer + full_len, leftover);
        buf_len = leftover;
        buffer[buf_len] = '\0';

        free(broadcast);
    }

    return NULL;
}

// === Cleanup ===
void cleanup_client(void)
{
    if (fd_c2s >= 0)
        close(fd_c2s);
    if (fd_s2c >= 0)
        close(fd_s2c);
    if (permission)
        free(permission);
    if (local_log)
        free(local_log);
    if (local_doc)
        markdown_free(local_doc);
}
