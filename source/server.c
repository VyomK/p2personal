#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "array_list.h"
#include "document.h"
#include "markdown.h"
#include "ipc_helpers.h"

#define MAX_FIFO_NAME 64

document *global_doc = NULL;
pthread_mutex_t doc_mutex = PTHREAD_MUTEX_INITIALIZER;

array_list *connected_clients;
pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;

array_list *global_cmd_list;
pthread_mutex_t cmd_list_mutex = PTHREAD_MUTEX_INITIALIZER;

char *server_log = NULL;
size_t server_log_len = 0;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

char *current_log_entry = NULL;
size_t current_log_len = 0;
size_t current_log_cap = 0;

uint64_t global_version = 1;
unsigned long time_interval_ms;

void handle_sig(int sig, siginfo_t *info, void *context);
void *client_thread(void *arg);

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <TIME_INTERVAL_MS>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    time_interval_ms = strtoul(argv[1], NULL, 10);
    printf("Server PID: %d\n", getpid());

    global_doc = markdown_init();
    connected_clients = create_array(8);
    global_cmd_list = create_array(16);
    server_log = Calloc(1, 1);

    struct sigaction sa = {0};
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handle_sig;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGRTMIN, &sa, NULL);

    while (1)
    {
        sleep_ms(time_interval_ms);
        handle_server_stdin();

        pthread_mutex_lock(&cmd_list_mutex);
        size_t cmd_count = global_cmd_list->size;
        pthread_mutex_unlock(&cmd_list_mutex);

        bool success_occured = false;
        uint64_t broadcast_version = global_version;

        if (cmd_count != 0)
        {
            reset_log_buffer();
            
            pthread_mutex_lock(&doc_mutex);
            pthread_mutex_lock(&cmd_list_mutex);
            for (size_t i = 0; i < global_cmd_list->size; i++)
            {
                cmd_ipc *c = (cmd_ipc *)get_from(global_cmd_list, i);
                int status = process_raw_command(global_doc, c);

                if (status == SUCCESS)
                    success_occured = true;

                const char *result_str = NULL;

                switch (status)
                {
                case SUCCESS:
                    result_str = "SUCCESS";
                    break;
                case INVALID_CURSOR_POS:
                    result_str = "Reject INVALID_POSITION";
                    break;
                case DELETED_POSITION:
                    result_str = "Reject DELETED_POSITION";
                    break;
                case REJECT_UNAUTHORISED:
                    result_str = "Reject UNAUTHORISED";
                    break;
                default:
                    result_str = "REJECT UNKNOWN_ERROR";
                    break;
                }

                int n = snprintf(NULL, 0, "EDIT %s %s %s\n", c->username, c->raw_command, result_str);
                char *line = Calloc(n + 1, 1);
                snprintf(line, n + 1, "EDIT %s %s %s\n", c->username, c->raw_command, result_str);
                append_to_log_buffer(line, n);
                free(line);
            }

            markdown_increment_version(global_doc);
            if (success_occured) {
                global_version++;
                broadcast_version = global_version;
            }

            for (size_t i = 0; i < global_cmd_list->size; i++)
            {
                free_cmd_ipc(get_from(global_cmd_list, i));
            }
            global_cmd_list = clear_array(global_cmd_list);

            pthread_mutex_unlock(&cmd_list_mutex);
            pthread_mutex_unlock(&doc_mutex);

            char version_line[64];
            int version_len = snprintf(version_line, sizeof(version_line),
                                       "VERSION %llu\n", (unsigned long long)broadcast_version);

            memmove(current_log_entry + version_len, current_log_entry, current_log_len);
            memcpy(current_log_entry, version_line, version_len);
            current_log_len += version_len;

            append_to_log_buffer("END\n", 4);

            append_to_server_log();
            
        }
        else
        {

            reset_log_buffer();
            current_log_len = snprintf(current_log_entry, current_log_cap,
                                       "VERSION %llu\nEND\n", (unsigned long long)global_version);
            append_to_server_log(); 
        }

        send_broadcast_to_all_clients();
    }

    return 0;
}

void handle_sig(int sig, siginfo_t *info, void *context)
{
    (void)sig;
    (void)context;
    pid_t client_pid = info->si_pid;
    pthread_t tid;
    int *arg = Calloc(1, sizeof(int));
    *arg = client_pid;
    pthread_create(&tid, NULL, client_thread, arg);
    pthread_detach(tid);
}

void *client_thread(void *arg)
{
    int client_pid = *((int *)arg);
    free(arg);

    char fifo_c2s[MAX_FIFO_NAME], fifo_s2c[MAX_FIFO_NAME];
    snprintf(fifo_c2s, sizeof(fifo_c2s), "FIFO_C2S_%d", client_pid);
    snprintf(fifo_s2c, sizeof(fifo_s2c), "FIFO_S2C_%d", client_pid);

    unlink(fifo_c2s);
    unlink(fifo_s2c);
    mkfifo(fifo_c2s, 0666);
    mkfifo(fifo_s2c, 0666);

    sigqueue(client_pid, SIGRTMIN + 1, (union sigval){.sival_int = 0});

    int fd_c2s = open(fifo_c2s, O_RDONLY);
    int fd_s2c = open(fifo_s2c, O_WRONLY);

    char *username = NULL;
    char *role = NULL;
    get_user_role(&username, &role, fd_c2s);

    if (!username || !role)
    {
        dprintf(fd_s2c, "Reject UNAUTHORISED\n");
        sleep(1);
        close(fd_c2s);
        close(fd_s2c);
        unlink(fifo_c2s);
        unlink(fifo_s2c);
        free(username);
        free(role);
        return NULL;
    }

    dprintf(fd_s2c, "%s\n", role);
    pthread_mutex_lock(&doc_mutex);
    dprintf(fd_s2c, "%llu\n", (unsigned long long)global_version);
    dprintf(fd_s2c, "%zu\n", global_doc->snapshot_len);
    write(fd_s2c, global_doc->snapshot, global_doc->snapshot_len);
    pthread_mutex_unlock(&doc_mutex);

    client_info *cinfo = Calloc(1, sizeof(client_info));
    cinfo->pid = client_pid;
    cinfo->fd_s2c = fd_s2c;
    cinfo->username = username;
    cinfo->permission = role;

    pthread_mutex_lock(&client_list_mutex);
    append_to(connected_clients, cinfo);
    pthread_mutex_unlock(&client_list_mutex);

    while (1)
    {
        char *line = read_line_dynamic(fd_c2s);
        if (!line)
            break;

        if (strcmp(line, "DISCONNECT") == 0)
        {
            free(line);
            break;
        }

        cmd_ipc *cmd = Calloc(1, sizeof(cmd_ipc));
        cmd->username = strdup(cinfo->username);
        cmd->role = strdup(cinfo->permission);
        cmd->raw_command = line;
        gettimeofday(&cmd->timestamp, NULL);
        

        insert_sorted_cmd(cmd);
    }

    close(fd_c2s);
    close(fd_s2c);
    unlink(fifo_c2s);
    unlink(fifo_s2c);

    pthread_mutex_lock(&client_list_mutex);
    remove_from(connected_clients, cinfo);
    pthread_mutex_unlock(&client_list_mutex);

    free(cinfo->username);
    free(cinfo->permission);
    free(cinfo);

    return NULL;
}
