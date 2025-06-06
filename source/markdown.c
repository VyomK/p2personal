#include "markdown.h"
#include "memory.h"
#include "document.h"
#include "array_list.h"
#include <string.h>
#include <stdbool.h>

#define SUCCESS 0
#define INVALID_CURSOR_POS -1
#define DELETED_POSITION -2

// === Init and Free ===
document *markdown_init(void)
{
    document *doc = (document *)Calloc(1, sizeof(document));
    doc->head = NULL;
    doc->tail = NULL;
    doc->num_characters = 0;
    doc->num_chunks = 0;

    doc->snapshot = NULL;
    doc->snapshot_len = 0;

    doc->meta_log = create_array(64);
    doc->cmd_list = create_array(64);
    doc->deleted_ranges = create_array(64);

    return doc;
}

void markdown_free(document *doc)
{
    if (!doc)
        return;

    Chunk *curr = doc->head;
    while (curr)
    {
        Chunk *temp = curr;
        curr = curr->next;
        free_chunk(temp);
    }

    free(doc->snapshot);

    free_array(doc->meta_log);
    free_array(doc->cmd_list);
    free_array(doc->deleted_ranges);

    free(doc);
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
    c->content = strdup(content);

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
    if (pos > doc->snapshot_len)
        return INVALID_CURSOR_POS;

    if (pos + len > doc->snapshot_len)
        len = doc->snapshot_len - pos;
    if (len == 0)
        return SUCCESS;

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

            free(remove_at(doc->deleted_ranges, i));
            i = -1; // restart since array has shifted
        }
    }

    append_to(doc->deleted_ranges, new_range);
    return SUCCESS;
}

// === Formatting Commands ===
int markdown_newline(document *doc, uint64_t version, size_t pos)
{

    (void)version;
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
    (void)version;
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
    (void)version;
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
    (void)version;
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
    (void)version;
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

    append_to(doc->cmd_list, c);

    return SUCCESS;
}

int markdown_horizontal_rule(document *doc, uint64_t version, size_t pos)
{
    (void)version;
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
    (void)version;
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
    c->content = strdup(url);

    append_to(doc->cmd_list, c);

    return SUCCESS;
}

// === Utilities ===
void markdown_print(const document *doc, FILE *stream)
{
    if (!doc || !stream)
        return;

    fwrite(doc->snapshot, sizeof(char), doc->snapshot_len, stream);
}

char *markdown_flatten(const document *doc)
{
    if (!doc || !doc->snapshot)
        return Calloc(1, sizeof(char));

    size_t len = doc->snapshot_len;
    char *copy = Calloc(len + 1, sizeof(char));
    memcpy(copy, doc->snapshot, len + 1);
    return copy;
}

// === Versioning ===

void markdown_increment_version(document *doc)
{
    if (!doc)
        return;

    // 1. Apply all deletions
    for (size_t i = 0; i < doc->deleted_ranges->size; ++i)
    {
        range *r = (range *)get_from(doc->deleted_ranges, i);
        naive_delete(doc, r->start, r->end - r->start);
    }

    // 2. Apply all insertions
    for (size_t i = 0; i < doc->cmd_list->size; ++i)
    {
        cmd *c = (cmd *)get_from(doc->cmd_list, i);

        switch (c->type)
        {
        case CMD_INSERT:
            naive_insert(doc, c->snap_pos, c->content);
            break;

        case CMD_NEWLINE:
            naive_newline(doc, c->snap_pos);
            break;

        case CMD_BLOCK_HEADING:
            naive_heading(doc, c->heading_level, c->snap_pos);
            break;

        case CMD_BLOCK_BLOCKQUOTE:
            naive_blockquote(doc, c->snap_pos);
            break;

        case CMD_BLOCK_OL_ITEM:
            naive_ordered_list(doc, c->snap_pos);
            break;

        case CMD_BLOCK_UL_ITEM:
            naive_unordered_list(doc, c->snap_pos);
            break;

        case CMD_BLOCK_HRULE:
            naive_horizontal_rule(doc, c->snap_pos);
            break;

        case CMD_INLINE_BOLD:
            naive_bold(doc, c->snap_pos, c->end_pos);
            break;

        case CMD_INLINE_ITALIC:
            naive_italic(doc, c->snap_pos, c->end_pos);
            break;

        case CMD_INLINE_CODE:
            naive_code(doc, c->snap_pos, c->end_pos);
            break;

        case CMD_INLINE_LINK:
            naive_link(doc, c->snap_pos, c->end_pos, c->content);
            break;

        default:
            break;
        }
    }

    // 3. Flatten and commit new snapshot
    if (doc->snapshot)
        free(doc->snapshot);
    doc->snapshot = flatten_document(doc);
    doc->snapshot_len = doc->num_characters;

    // 4. Clear metadata
    doc->meta_log = clear_array(doc->meta_log);
    doc->cmd_list = clear_array(doc->cmd_list);
    doc->deleted_ranges = clear_array(doc->deleted_ranges);

    return;
}
