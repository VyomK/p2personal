#define _POSIX_C_SOURCE 200809L

#define BUILD_CLIENT

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ipc_helpers.h"
#include "memory.h"

void infer_chunk_type(const char *line, size_t len, chunk_type *type_out, int *index_OL_out)
{
    *type_out = PLAIN;
    *index_OL_out = 0;

    if (len == 4 && strncmp(line, "---\n", 4) == 0)
        *type_out = HORIZONTAL_RULE;
    else if (len >= 2 && line[0] == '>' && line[1] == ' ')
        *type_out = BLOCKQUOTE;
    else if (len >= 2 && line[0] == '-' && line[1] == ' ')
        *type_out = UNORDERED_LIST_ITEM;
    else if (len >= 2 && line[0] == '#' && line[1] == ' ')
        *type_out = HEADING1;
    else if (len >= 3 && line[0] == '#' && line[1] == '#' && line[2] == ' ')
        *type_out = HEADING2;
    else if (len >= 4 && line[0] == '#' && line[1] == '#' && line[2] == '#' && line[3] == ' ')
        *type_out = HEADING3;
    else if (len >= 3 && line[1] == '.' && line[2] == ' ' && line[0] >= '1' && line[0] <= '9') {
        *type_out = ORDERED_LIST_ITEM;
        *index_OL_out = line[0] - '0';
    }
}

void markdown_parse_string(document *doc, const char *text)
{
    if (!doc || !text) return;

    const char *start = text, *cursor = text;

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
        else cursor++;
    }

    if (cursor > start) {
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
    char *line = strtok(copy, "\n");

    while (line) {
        if (strncmp(line, "EDIT ", 5) == 0) {
            char *edit_ptr = line + 5;
            char *success_ptr = strstr(edit_ptr, " SUCCESS");
            if (success_ptr) {
                *success_ptr = '\0';
                char *cmd_start = strchr(edit_ptr, ' ');
                if (cmd_start && *(cmd_start + 1)) {
                    cmd_start++;
                    char *raw = strdup(cmd_start);
                    process_raw_command(local_doc, &(cmd_ipc){ .raw_command = raw, .role = "write" });
                    free(raw);
                }
            }
        }
        line = strtok(NULL, "\n");
    }

    free(copy);
}
