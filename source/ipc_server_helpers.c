#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include "ipc_helpers.h"
#include "memory.h"
#include "markdown.h"

#define INITIAL_CAPACITY 512

#include <time.h>

void sleep_ms(unsigned long milliseconds)
{
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000UL;
    nanosleep(&ts, NULL);
}

void handle_server_stdin(void)
{
    fd_set set;
    struct timeval timeout = {0, 0};
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);

    if (select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout) <= 0)
    {
        return; 
    }

    char *line = NULL;
    size_t len = 0;

    if (getline(&line, &len, stdin) == -1)
    {
        free(line);
        return;
    }

    if (strcmp(line, "DOC?\n") == 0)
    {
        pthread_mutex_lock(&doc_mutex);
        char *flattened = markdown_flatten(global_doc);
        fputs(flattened, stdout);
        fflush(stdout);
        free(flattened);
        pthread_mutex_unlock(&doc_mutex);
    }
    else if (strcmp(line, "LOG?\n") == 0)
    {
        pthread_mutex_lock(&log_mutex);
        fwrite(server_log, 1, server_log_len, stdout);
        fflush(stdout);
        pthread_mutex_unlock(&log_mutex);
    }
    else if (strcmp(line, "QUIT?\n") == 0)
    {
        pthread_mutex_lock(&client_list_mutex);
        size_t num_clients = connected_clients->size;
        pthread_mutex_unlock(&client_list_mutex);

        if (num_clients == 0)
        {
            FILE *f = fopen("doc.md", "w");
            if (f != NULL)
            {
                pthread_mutex_lock(&doc_mutex);
                char *flattened = markdown_flatten(global_doc);
                fwrite(flattened, 1, strlen(flattened), f);
                free(flattened);
                pthread_mutex_unlock(&doc_mutex);
                fclose(f);
            }
            free_server_resources();
            exit(0);
        }
        else
        {
            printf("QUIT rejected, %zu clients still connected.\n", num_clients);
            fflush(stdout);
        }
    }

    free(line);
}

void free_cmd_ipc(void *ptr)
{
    cmd_ipc *cmd = (cmd_ipc *)ptr;
    if (!cmd)
        return;
    free(cmd->username);
    free(cmd->role);
    free(cmd->raw_command);
}

void free_server_resources(void)
{
    if (global_doc)
    {
        markdown_free(global_doc);
        global_doc = NULL;
    }

    if (global_cmd_list)
    {
        for (size_t i = 0; i < global_cmd_list->size; i++)
            free_cmd_ipc(get_from(global_cmd_list, i));
        free_array(global_cmd_list);
        global_cmd_list = NULL;
    }

    if (connected_clients)
    {
        for (size_t i = 0; i < connected_clients->size; i++)
        {
            client_info *c = get_from(connected_clients, i);
            free(c->username);
            free(c->permission);
        }
        free_array(connected_clients);
        connected_clients = NULL;
    }

    free(server_log);
    server_log = NULL;
    free(current_log_entry);
    current_log_entry = NULL;
}

void reset_log_buffer(void)
{
    if (!current_log_entry)
    {
        current_log_cap = INITIAL_CAPACITY;
        current_log_entry = Calloc(current_log_cap, sizeof(char));
    }
    current_log_len = 0;
    current_log_entry[0] = '\0';
}

void append_to_log_buffer(const char *data, size_t len)
{
    if (current_log_len + len + 1 > current_log_cap)
    {
        current_log_cap = (current_log_len + len + 1) * 2;
        current_log_entry = realloc(current_log_entry, current_log_cap);
    }
    memcpy(current_log_entry + current_log_len, data, len);
    current_log_len += len;
    current_log_entry[current_log_len] = '\0';
}

void append_to_server_log(void)
{
    pthread_mutex_lock(&log_mutex);
    server_log = realloc(server_log, server_log_len + current_log_len + 1);
    memcpy(server_log + server_log_len, current_log_entry, current_log_len);
    server_log_len += current_log_len;
    server_log[server_log_len] = '\0';
    pthread_mutex_unlock(&log_mutex);
}

void send_broadcast_to_all_clients(void)
{
    pthread_mutex_lock(&client_list_mutex);
    for (size_t i = 0; i < connected_clients->size; i++)
    {
        client_info *c = get_from(connected_clients, i);
        write(c->fd_s2c, current_log_entry, current_log_len);
    }
    pthread_mutex_unlock(&client_list_mutex);
}

char *trim(char *str)
{
    while (isspace((unsigned char)*str))
        str++;
    if (*str == 0)
        return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';
    return str;
}

void get_user_role(char **username_out, char **role_out, int fd)
{
    char *raw = read_line_dynamic(fd);
    if (!raw)
    {
        *username_out = NULL;
        *role_out = NULL;
        return;
    }

    *username_out = strdup(trim(raw));
    free(raw);

    FILE *f = fopen("roles.txt", "r");
    if (!f)
    {
        *role_out = NULL;
        return;
    }

    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, f) != -1)
    {
        char *saveptr = NULL;
        char *user = strtok_r(line, " \t\r\n", &saveptr);
        char *role = strtok_r(NULL, " \t\r\n", &saveptr);
        if (!user || !role)
            continue;
        if (strcmp(trim(user), *username_out) == 0 &&
            (strcmp(role, "read") == 0 || strcmp(role, "write") == 0))
        {
            *role_out = strdup(trim(role));
            fclose(f);
            free(line);
            return;
        }
    }

    free(line);
    fclose(f);
    *role_out = NULL;
}

void insert_sorted_cmd(cmd_ipc *cmd)
{
    pthread_mutex_lock(&cmd_list_mutex);
    append_to(global_cmd_list, cmd);
    int i = global_cmd_list->size - 1;
    while (i > 0)
    {
        cmd_ipc *curr = get_from(global_cmd_list, i);
        cmd_ipc *prev = get_from(global_cmd_list, i - 1);
        int earlier = (curr->timestamp.tv_sec < prev->timestamp.tv_sec) ||
                      (curr->timestamp.tv_sec == prev->timestamp.tv_sec &&
                       curr->timestamp.tv_usec < prev->timestamp.tv_usec);
        if (!earlier)
            break;
        global_cmd_list->data[i] = prev;
        global_cmd_list->data[i - 1] = curr;
        i--;
    }
    pthread_mutex_unlock(&cmd_list_mutex);
}
