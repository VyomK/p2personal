

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>


#include "ipc_helpers.h"
#include "document.h"
#include "markdown.h"

#define INITIAL_CAPACITY 512

// === Shared ===

char *read_line_dynamic(int fd)
{
    size_t cap = 64;
    size_t len = 0;
    char *buf = Calloc(cap, 1);

    while (1)
    {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r <= 0)
        {
            free(buf);
            return NULL;
        }
        if (c == '\n')
            break;

        if (len + 1 >= cap)
        {
            cap *= 2;
            buf = realloc(buf, cap);
        }
        buf[len++] = c;
    }

    buf[len] = '\0';
    return buf;
}

// === Server helpers ===

void handle_server_stdin(void)
{
    char *line = NULL;
    size_t len = 0;

    if (getline(&line, &len, stdin) == -1) {
        free(line);
        return;
    }

    if (strcmp(line, "DOC?\n") == 0) {
        pthread_mutex_lock(&doc_mutex);
        markdown_print(global_doc, stdout);
        pthread_mutex_unlock(&doc_mutex);
    }
    else if (strcmp(line, "LOG?\n") == 0) {
        pthread_mutex_lock(&log_mutex); 
        fwrite(server_log, 1, server_log_len, stdout);
        pthread_mutex_unlock(&log_mutex);
    }
    else if (strcmp(line, "QUIT?\n") == 0) {
        pthread_mutex_lock(&client_list_mutex);
        size_t num_clients = connected_clients->size;
        pthread_mutex_unlock(&client_list_mutex);

        if (num_clients == 0) {
            FILE *f = fopen("doc.md", "w");
            if (f) {
                pthread_mutex_lock(&doc_mutex);
                markdown_print(global_doc, f);
                pthread_mutex_unlock(&doc_mutex);
                fclose(f);
            }
            // cleanup() could free global_doc, arrays, etc.
            free_server_resources();
            exit(0);
        } else {
            printf("QUIT rejected, %zu clients still connected.\n", num_clients);
        }
    }

    free(line);
}

int process_raw_command(document *doc, cmd_ipc *cmd)
{
    if (!doc || !cmd || !cmd->raw_command)
        return INTERNAL_ERROR;

    if (strcmp(cmd->role, "read") == 0)
    {
        return REJECT_UNAUTHORISED;
    }

    char *line = strdup(cmd->raw_command);
    char *saveptr = NULL;
    char *type = strtok_r(line, " ", &saveptr);

    int result = INTERNAL_ERROR;

    if (strcmp(type, "INSERT") == 0)
    {
        size_t pos = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        char *text = strtok_r(NULL, "", &saveptr);
        result = markdown_insert(doc, NULL, pos, text);
    }
    else if (strcmp(type, "DEL") == 0)
    {
        size_t pos = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        size_t len = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        result = markdown_delete(doc, NULL, pos, len);
    }
    else if (strcmp(type, "NEWLINE") == 0)
    {
        size_t pos = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        result = markdown_newline(doc, NULL, pos);
    }
    else if (strcmp(type, "HEADING") == 0)
    {
        int level = atoi(strtok_r(NULL, " ", &saveptr));
        size_t pos = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        result = markdown_heading(doc, NULL, level, pos);
    }
    else if (strcmp(type, "BOLD") == 0)
    {
        size_t start = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        size_t end = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        result = markdown_bold(doc, NULL, start, end);
    }
    else if (strcmp(type, "ITALIC") == 0)
    {
        size_t start = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        size_t end = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        result = markdown_italic(doc, NULL, start, end);
    }
    else if (strcmp(type, "CODE") == 0)
    {
        size_t start = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        size_t end = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        result = markdown_code(doc, NULL, start, end);
    }
    else if (strcmp(type, "BLOCKQUOTE") == 0)
    {
        size_t pos = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        result = markdown_blockquote(doc, NULL, pos);
    }
    else if (strcmp(type, "ORDERED_LIST") == 0)
    {
        size_t pos = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        result = markdown_ordered_list(doc, NULL, pos);
    }
    else if (strcmp(type, "UNORDERED_LIST") == 0)
    {
        size_t pos = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        result = markdown_unordered_list(doc, NULL, pos);
    }
    else if (strcmp(type, "HORIZONTAL_RULE") == 0)
    {
        size_t pos = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        result = markdown_horizontal_rule(doc, NULL, pos);
    }
    else if (strcmp(type, "LINK") == 0)
    {
        size_t start = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        size_t end = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        char *link = strtok_r(NULL, "", &saveptr);
        result = markdown_link(doc, NULL, start, end, link);
    }

    free(line);
    return result;
}

void free_cmd_ipc(void *ptr) {
    cmd_ipc *cmd = (cmd_ipc *)ptr;
    if (!cmd) return;
    free(cmd->username);
    free(cmd->role);
    free(cmd->raw_command);
}

void free_server_resources(void) {
    if (global_doc) {
        markdown_free(global_doc);
        global_doc = NULL;
    }

    if (global_cmd_list) {
        for (int i = 0; i < global_cmd_list->size; i++) {
            free_cmd_ipc(get_from(global_cmd_list, i));
        }
        free_array(global_cmd_list);  
        global_cmd_list = NULL;
    }

    if (connected_clients) {
        for (int i = 0; i < connected_clients->size; i++) {
            client_info *c = get_from(connected_clients, i);
            free(c->username);
            free(c->permission);
        }
        free_array(connected_clients);  // only frees pointers
        connected_clients = NULL;
    }

    if (server_log) {
        free(server_log);
        server_log = NULL;
    }

    if (current_log_entry) {
        free(current_log_entry);
        current_log_entry = NULL;
    }
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
    for (int i = 0; i < connected_clients->size; i++)
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

    // Store trimmed username as output
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

        char *tuser = trim(user);
        char *trole = trim(role);

        if (strcmp(tuser, *username_out) == 0 &&
            (strcmp(trole, "read") == 0 || strcmp(trole, "write") == 0))
        {
            *role_out = strdup(trole);
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
        cmd_ipc *curr = (cmd_ipc *)global_cmd_list->data[i];
        cmd_ipc *prev = (cmd_ipc *)global_cmd_list->data[i - 1];

        int earlier =
            (curr->timestamp.tv_sec < prev->timestamp.tv_sec) ||
            (curr->timestamp.tv_sec == prev->timestamp.tv_sec &&
             curr->timestamp.tv_usec < prev->timestamp.tv_usec);

        if (!earlier)
            break;

        // Swap
        global_cmd_list->data[i] = prev;
        global_cmd_list->data[i - 1] = curr;
        i--;
    }

    pthread_mutex_unlock(&cmd_list_mutex);
}

// === client helpers===
void infer_chunk_type(const char *line, size_t len, chunk_type *type_out, int *index_OL_out)
{
    *type_out = PLAIN;
    *index_OL_out = 0;

    if (len == 4 && strncmp(line, "---\n", 4) == 0) {
        *type_out = HORIZONTAL_RULE;
    }
    else if (len >= 2 && line[0] == '>' && line[1] == ' ') {
        *type_out = BLOCKQUOTE;
    }
    else if (len >= 2 && line[0] == '-' && line[1] == ' ') {
        *type_out = UNORDERED_LIST_ITEM;
    }
    else if (len >= 2 && line[0] == '#' && line[1] == ' ') {
        *type_out = HEADING1;
    }
    else if (len >= 3 && line[0] == '#' && line[1] == '#' && line[2] == ' ') {
        *type_out = HEADING2;
    }
    else if (len >= 4 && line[0] == '#' && line[1] == '#' && line[2] == '#' && line[3] == ' ') {
        *type_out = HEADING3;
    }
    else if (len >= 3 && line[1] == '.' && line[2] == ' ' && line[0] >= '1' && line[0] <= '9') {
        *type_out = ORDERED_LIST_ITEM;
        *index_OL_out = line[0] - '0';
    }
}

void markdown_parse_string(document *doc, const char *text)
{
    if (!doc || !text) return;

    const char *start = text;
    const char *cursor = text;

    while (*cursor)
    {
        if (*cursor == '\n')
        {
            size_t len = cursor - start;
            char *line = Calloc(len + 2, sizeof(char));
            memcpy(line, start, len);
            line[len] = '\n';
            line[len + 1] = '\0';

            size_t cap = calculate_cap(len + 1);
            chunk_type type;
            int index_OL;
            infer_chunk_type(line, len + 1, &type, &index_OL);

            Chunk *chunk = Calloc(1, sizeof(Chunk));
            init_chunk(chunk, type, len + 1, cap, line, index_OL, NULL, NULL);

            if (!doc->head)
                doc->head = doc->tail = chunk;
            else {
                doc->tail->next = chunk;
                chunk->previous = doc->tail;
                doc->tail = chunk;
            }

            doc->num_chunks++;
            doc->num_characters += chunk->len;

            cursor++;
            start = cursor;
        }
        else
        {
            cursor++;
        }
    }

    if (cursor > start)
    {
        size_t len = cursor - start;
        char *line = Calloc(len + 2, sizeof(char));
        memcpy(line, start, len);
        line[len] = '\n';
        line[len + 1] = '\0';

        size_t cap = calculate_cap(len + 1);
        chunk_type type;
        int index_OL;
        infer_chunk_type(line, len + 1, &type, &index_OL);

        Chunk *chunk = Calloc(1, sizeof(Chunk));
        init_chunk(chunk, type, len + 1, cap, line, index_OL, NULL, NULL);

        if (!doc->head)
            doc->head = doc->tail = chunk;
        else {
            doc->tail->next = chunk;
            chunk->previous = doc->tail;
            doc->tail = chunk;
        }

        doc->num_chunks++;
        doc->num_characters += chunk->len;
    }
}

void apply_broadcast(const char *msg)
{
    if (!msg || !local_doc)
        return;

    char *copy = strdup(msg);
    char *line = NULL;
    char *saveptr = NULL;

    line = strtok_r(copy, "\n", &saveptr);
    while (line) {
        if (strncmp(line, "EDIT ", 5) == 0) {
            char *edit_ptr = line + 5; // Skip "EDIT "

            // Extract command part-> find last space before "SUCCESS"
            char *success_ptr = strstr(edit_ptr, " SUCCESS");
            if (success_ptr) {
                *success_ptr = '\0';  // Terminate just before " SUCCESS"

                // Skip username-> find first space
                char *cmd_start = strchr(edit_ptr, ' ');
                if (cmd_start && *(cmd_start + 1)) {
                    cmd_start++;

                    // Reuse process_raw_command logic
                    char *raw = strdup(cmd_start);

                    // Directly apply (role doesn't matter here)
                    int res = process_raw_command(local_doc,
                        &(cmd_ipc){.raw_command = raw, .role = "write"});

                    if (res < 0) {
                        fprintf(stderr, "[client] Failed to apply command: %s\n", cmd_start);
                    }

                    free(raw);
                }
            }
        }

        // Ignore VERSION, END, Reject lines
        line = strtok_r(NULL, "\n", &saveptr);
    }

    free(copy);
}
