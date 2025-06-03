#ifndef IPC_HELPERS_H
#define IPC_HELPERS_H

#define REJECT_UNAUTHORISED 1001
#define INTERNAL_ERROR 1002

#include <stddef.h>

//== server forward declarations ===

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

//== server forward declarations ===

extern local_doc;



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

// === Shared Helper ===
char *read_line_dynamic(int fd);

// === Server Helpers ===
void handle_server_stdin(void);
int process_raw_command(document *doc, cmd_ipc *cmd);
void insert_sorted_cmd(cmd_ipc *cmd);
void free_cmd_ipc(void *ptr);
void free_server_resources(void);

void reset_log_buffer(void);
void append_to_log_buffer(const char *data, size_t len);
void append_to_server_log(void);
void send_broadcast_to_all_clients(void);

char *trim(char *str);
void get_user_role(char **username_out, char **role_out, int fd);

// === Client Helpers ===

#endif