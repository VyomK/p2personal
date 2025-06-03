#ifndef IPC_HELPERS_H
#define IPC_HELPERS_H

#define REJECT_UNAUTHORISED 1001
#define INTERNAL_ERROR 1002

#include <stddef.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include "document.h"

// === Shared declarations ===
typedef struct cmd_ipc {
    char *username;
    char *role;
    char *raw_command;
    struct timeval timestamp;
} cmd_ipc;

typedef struct client_info {
    pid_t pid;
    int fd_s2c;
    char *username;
    char *permission;
} client_info;

char *read_line_dynamic(int fd);
int process_raw_command(document *doc, cmd_ipc *cmd);

// === Server-side globals ===
#ifndef BUILD_CLIENT
extern document *global_doc;
extern pthread_mutex_t doc_mutex;

extern array_list *connected_clients;
extern pthread_mutex_t client_list_mutex;

extern array_list *global_cmd_list;
extern pthread_mutex_t cmd_list_mutex;

extern char *current_log_entry;
extern size_t current_log_len;
extern size_t current_log_cap;

extern char *server_log;
extern size_t server_log_len;
extern pthread_mutex_t log_mutex;

extern uint64_t global_version;

// === Server-side helpers ===
void sleep_ms(unsigned long milliseconds);
void handle_server_stdin(void);
void insert_sorted_cmd(cmd_ipc *cmd);
void free_cmd_ipc(void *ptr);
void free_server_resources(void);

void reset_log_buffer(void);
void append_to_log_buffer(const char *data, size_t len);
void append_to_server_log(void);
void send_broadcast_to_all_clients(void);

char *trim(char *str);
void get_user_role(char **username_out, char **role_out, int fd);
#endif

// === Client-side globals and helpers ===
#ifdef BUILD_CLIENT
extern document *local_doc;
extern pthread_mutex_t local_doc_mutex;
extern char *local_log;
extern pthread_mutex_t local_log_mutex;

void markdown_parse_string(document *doc, const char *text);
void infer_chunk_type(const char *line, size_t len, chunk_type *type_out, int *index_OL_out);
void apply_broadcast(const char *msg);
#endif

#endif // IPC_HELPERS_H
