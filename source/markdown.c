#include "markdown.h"
#include "memory.h"
#include "document.h"
#include "array_list.h"
#include <stdbool.h>

#define SUCCESS 0
#define INVALID_CURSOR_POS -1
#define DELETED_POSITION -2
#define OUTDATED_VERSION -3 /*IGNORE: COMP9017*/

// === Init and Free ===
document *markdown_init(void)
{

    document *doc = (document *)Calloc(1, sizeof(document));
    doc->head = NULL;
    doc->tail = NULL;
    doc->num_characters = 0;
    doc->num_chunks = 0;
    doc->version = 0;

    return doc;
}

void markdown_free(document *doc)
{
    if (doc)
    {
        Chunk *curr = doc->head;

        while (curr)
        {
            Chunk *temp = curr;
            curr = curr->next;
            free_chunk(temp);
        }

        free(doc);
    }
}

// === Edit Commands ===
int markdown_insert(document *doc, uint64_t version, size_t pos, const char *content)
{
    (void)version;
    if (!doc || !content || pos > doc->snapshot_len)
        return INVALID_CURSOR_POS;

    cmd *c = Calloc(1, sizeof(cmd));
    c->type = CMD_INSERT;
    c->snap_pos = pos;
    c->content = content;

    append_to(doc->cmd_list, c);
    return SUCCESS;
}

int markdown_delete(document *doc,
                    uint64_t version,
                    size_t pos,
                    size_t len)
{
    (void)version;

    if (!doc)
        return INVALID_CURSOR_POS;
    if (len == 0)
        return SUCCESS;
    if (pos > doc->snapshot_len || pos < 0)
        return INVALID_CURSOR_POS;

    range *new_range = Calloc(1, sizeof(range));
    new_range->start = pos;
    new_range->end = pos + len;

    for (size_t i = 0; i < doc->deleted_ranges->size; ++i)
    {
        range *r = (range *)get_from(doc->deleted_ranges, i);

        if (new_range->start <= r->end && r->start <= new_range->end)
        {
            // Merge r into new_range
            if (r->start < new_range->start)
                new_range->start = r->start;
            if (r->end > new_range->end)
                new_range->end = r->end;

            free(remove_at(doc->deleted_ranges, i)); // remove and free old
            i = -1;                                  // restart since array has shifted
        }
    }

    append_to(doc->deleted_ranges, new_range);
    return SUCCESS;
}

// === Formatting Commands ===
int markdown_newline(document *doc, uint64_t version, size_t pos)
{

    if (!doc || pos > doc->snapshot_len)
        return INVALID_CURSOR_POS;

    cmd *c = Calloc(1, sizeof(cmd));
    c->type = CMD_NEWLINE;
    c->snap_pos = pos;

    append_to(doc->cmd_list, c);
    return SUCCESS;
}

int markdown_heading(document *doc, uint64_t version, size_t level, size_t pos)
{
    (void)version;

    if (!doc || pos > doc->snapshot_len || level < 1 || level > 3)
        return INVALID_CURSOR_POS;

    cmd *c = Calloc(1, sizeof(cmd));
    c->type = CMD_BLOCK_HEADING;
    c->snap_pos = pos;
    c->heading_level = level;

    append_to(doc->cmd_list, c);
    return SUCCESS;
}

int markdown_bold(document *doc, uint64_t version, size_t start, size_t end)
{
    if (!doc || start >= end || end > doc->snapshot_len)
    {
        return INVALID_CURSOR_POS;
    }

    range *r1 = clamp_to_valid(doc, start);
    range *r2 = clamp_to_valid(doc, end);

    if (r1 && r2)
        return DELETED_POSITION;

    cmd *c = Calloc(1, sizeof(cmd));
    c->type = CMD_INLINE_BOLD;
    c->snap_pos = start;
    c->end_pos = end;

    append_to(doc->cmd_list, c);
    return SUCCESS;
}

int markdown_italic(document *doc, uint64_t version, size_t start, size_t end)
{
    if (!doc || start >= end || end > doc->snapshot_len)
    {
        return INVALID_CURSOR_POS;
    }

    range *r1 = clamp_to_valid(doc, start);
    range *r2 = clamp_to_valid(doc, end);

    if (r1 && r2)
        return DELETED_POSITION;

    cmd *c = Calloc(1, sizeof(cmd));
    c->type = CMD_INLINE_ITALIC;
    c->snap_pos = start;
    c->end_pos = end;

    append_to(doc->cmd_list, c);
    return SUCCESS;
}

int markdown_blockquote(document *doc, uint64_t version, size_t pos)
{
    if (!doc || pos > doc->snapshot_len)
        return INVALID_CURSOR_POS;

    cmd *c = Calloc(1, sizeof(cmd));
    c->type = CMD_BLOCK_BLOCKQUOTE;
    c->snap_pos = pos;

    append_to(doc->cmd_list, c);
    return SUCCESS;
}

int markdown_ordered_list(document *doc,
                          uint64_t version,
                          size_t pos)
{
    (void)version;
    if (!doc || pos > doc->snapshot_len)
        return INVALID_CURSOR_POS;

    cmd *c = Calloc(1, sizeof(cmd));
    c->type = CMD_BLOCK_OL_ITEM;
    c->snap_pos = pos;

    append_to(doc->cmd_list, c);
    return SUCCESS;
}

int markdown_unordered_list(document *doc, uint64_t version, size_t pos)
{
    (void)version;
    if (!doc || pos > doc->snapshot_len)
        return INVALID_CURSOR_POS;

    cmd *c = Calloc(1, sizeof(cmd));
    c->type = CMD_BLOCK_UL_ITEM;
    c->snap_pos = pos;

    append_to(doc->cmd_list, c);
    return SUCCESS;
}

int markdown_code(document *doc, uint64_t version, size_t start, size_t end)
{
    if (!doc || start >= end || end > doc->snapshot_len)
    {
        return INVALID_CURSOR_POS;
    }

    range *r1 = clamp_to_valid(doc, start);
    range *r2 = clamp_to_valid(doc, end);

    if (r1 && r2)
    {
        return DELETED_POSITION;
    }

    cmd *c = Calloc(1, sizeof(cmd));
    c->type = CMD_INLINE_CODE;
    c->snap_pos = start;
    c->end_pos = end;

    return SUCCESS;
}

int markdown_horizontal_rule(document *doc, uint64_t version, size_t pos)
{
    if (!doc || pos > doc->snapshot_len)
        return INVALID_CURSOR_POS;

    cmd *c = Calloc(1, sizeof(cmd));
    c->type = CMD_BLOCK_HRULE;
    c->snap_pos = pos;

    append_to(doc->cmd_list, c);
    return SUCCESS;
}

int markdown_link(document *doc, uint64_t version, size_t start, size_t end, const char *url)
{
    if (!doc || start >= end || end > doc->snapshot_len)
    {
        return INVALID_CURSOR_POS;
    }

    range *r1 = clamp_to_valid(doc, start);
    range *r2 = clamp_to_valid(doc, end);

    if (r1 && r2)
        return DELETED_POSITION;

    cmd *c = Calloc(1, sizeof(cmd));
    c->type = CMD_INLINE_LINK;
    c->snap_pos = start;
    c->end_pos = end;
    c->content = url;

    append_to(doc->cmd_list, c);

    return SUCCESS;
}

// === Utilities ===
void markdown_print(const document *doc, FILE *stream)
{
    if (!doc || !stream)
    {
        return;
    }

    Chunk *curr = doc->head;
    while (curr)
    {
        fwrite(curr->text, sizeof(char), curr->len, stream);
        curr = curr->next;
    }
}

char *markdown_flatten(const document *doc)
{
    if (!doc || doc->num_characters == 0)
        return Malloc(1);

    char *output = Malloc(doc->num_characters + 1); // +1 for null terminator
    size_t offset = 0;

    for (Chunk *c = doc->head; c; c = c->next)
    {
        memcpy(output + offset, c->text, c->len);
        offset += c->len;
    }

    output[offset] = '\0';
    return output;
}

// === Versioning ===
void markdown_increment_version(document *doc)
{
    doc->version += 1;
}
