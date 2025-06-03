#define _POSIX_C_SOURCE 200809L


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "ipc_helpers.h"
#include "memory.h"
#include "document.h"
#include "markdown.h"

char *read_line_dynamic(int fd)
{
    size_t cap = 64;
    size_t len = 0;
    char *buf = Calloc(cap, 1);

    while (1)
    {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r <= 0) {
            free(buf);
            return NULL;
        }
        if (c == '\n')
            break;

        if (len + 1 >= cap) {
            cap *= 2;
            buf = realloc(buf, cap);
        }
        buf[len++] = c;
    }

    buf[len] = '\0';
    return buf;
}

int process_raw_command(document *doc, cmd_ipc *cmd)
{
    if (!doc || !cmd || !cmd->raw_command)
        return INTERNAL_ERROR;

    if (strcmp(cmd->role, "read") == 0)
        return REJECT_UNAUTHORISED;

    char *line = strdup(cmd->raw_command);
    char *saveptr = NULL;
    char *type = strtok_r(line, " ", &saveptr);
    int result = INTERNAL_ERROR;

    if (strcmp(type, "INSERT") == 0) {
        size_t pos = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        char *text = strtok_r(NULL, "", &saveptr);
        result = markdown_insert(doc, 0, pos, text);
    } else if (strcmp(type, "DEL") == 0) {
        size_t pos = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        size_t len = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        result = markdown_delete(doc, 0, pos, len);
    } else if (strcmp(type, "NEWLINE") == 0) {
        size_t pos = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        result = markdown_newline(doc, 0, pos);
    } else if (strcmp(type, "HEADING") == 0) {
        int level = atoi(strtok_r(NULL, " ", &saveptr));
        size_t pos = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        result = markdown_heading(doc, 0, level, pos);
    } else if (strcmp(type, "BOLD") == 0 || strcmp(type, "ITALIC") == 0 || strcmp(type, "CODE") == 0 || strcmp(type, "LINK") == 0) {
        size_t start = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        size_t end = (size_t)atoi(strtok_r(NULL, " ", &saveptr));
        if (strcmp(type, "BOLD") == 0)
            result = markdown_bold(doc, 0, start, end);
        else if (strcmp(type, "ITALIC") == 0)
            result = markdown_italic(doc, 0, start, end);
        else if (strcmp(type, "CODE") == 0)
            result = markdown_code(doc, 0, start, end);
        else {
            char *url = strtok_r(NULL, "", &saveptr);
            result = markdown_link(doc, 0, start, end, url);
        }
    } else if (strcmp(type, "BLOCKQUOTE") == 0)
        result = markdown_blockquote(doc, 0, (size_t)atoi(strtok_r(NULL, " ", &saveptr)));
    else if (strcmp(type, "ORDERED_LIST") == 0)
        result = markdown_ordered_list(doc, 0, (size_t)atoi(strtok_r(NULL, " ", &saveptr)));
    else if (strcmp(type, "UNORDERED_LIST") == 0)
        result = markdown_unordered_list(doc, 0, (size_t)atoi(strtok_r(NULL, " ", &saveptr)));
    else if (strcmp(type, "HORIZONTAL_RULE") == 0)
        result = markdown_horizontal_rule(doc, 0, (size_t)atoi(strtok_r(NULL, " ", &saveptr)));

    free(line);
    return result;
}
