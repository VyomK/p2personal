#include "markdown.h"
#include "memory.h"
#include "document.h"
#include "naive_ops.h"
#include <stdbool.h>

#define SUCCESS 0
#define INVALID_CURSOR_POS -1
#define DELETED_POSITION -2
#define OUTDATED_VERSION -3 /*IGNORE: COMP9017*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>


// === Edit Commands ===
int naive_insert(document *doc, size_t pos, const char *content)
{
    size_t snapshot_pos = pos;

    range *r = clamp_to_valid(doc, snapshot_pos);
    size_t effective_pos = r ? r->start : snapshot_pos;

    size_t working_pos = map_snapshot_to_working(doc->meta_log, effective_pos);

    return naive_insert_raw(doc, working_pos, snapshot_pos, content);
}

int naive_insert_raw(document *doc, size_t working_pos, size_t snapshot_pos, const char *content)
{
    size_t content_size = strlen(content);

    if (doc->head == NULL)
    {
        if (working_pos != 0)
            return INVALID_CURSOR_POS;

        Chunk *new_chunk = (Chunk *)Calloc(1, sizeof(Chunk));
        size_t cap = calculate_cap(content_size + 1);

        char *text = (char *)Calloc(cap, sizeof(char));
        text = memcpy(text, content, content_size + 1);
        init_chunk(new_chunk, PLAIN, content_size, cap, text, 0, NULL, NULL);

        doc->head = new_chunk;
        doc->tail = new_chunk;
        doc->num_characters = content_size;
        doc->num_chunks++;

        update_meta_log(doc->meta_log, snapshot_pos, content_size);
        return SUCCESS;
    }

    if (working_pos > doc->num_characters)
        return INVALID_CURSOR_POS;

    size_t local_pos;
    Chunk *curr = locate_chunk(doc, working_pos, &local_pos);

    chunk_insert(curr, local_pos, content, content_size);
    doc->num_characters += content_size;

    update_meta_log(doc->meta_log, snapshot_pos, content_size);
    return SUCCESS;
}

int naive_delete(document *doc, size_t pos, size_t len)
{

    if (!doc)
        return INVALID_CURSOR_POS;

    size_t snapshot_pos = pos;
    size_t snapshot_len = doc->snapshot_len;

    pos = map_snapshot_to_working(doc->meta_log, snapshot_pos);

    if (snapshot_pos > snapshot_len)
        return INVALID_CURSOR_POS;

    // Clamp deletion to not go beyond snapshot
    if (snapshot_pos + len > snapshot_len)
        len = snapshot_len - snapshot_pos;

    if (len == 0)
        return SUCCESS;

    if (pos > doc->num_characters)
        return INVALID_CURSOR_POS;

    /* 1) Locate & Analyze */
    size_t local_pos;
    Chunk *start = locate_chunk(doc, pos, &local_pos);
    if (!start)
        return INVALID_CURSOR_POS;

    bool ol_damaged = false;
    if (start->type == ORDERED_LIST_ITEM && local_pos < 3 && local_pos + len >= 3)
    {
        start->type = PLAIN;
        start->index_OL = 0;
        ol_damaged = true;
    }

    /* 2a) Fast‐path: delete wholly within one chunk */
    if (local_pos + len < start->len)
    {
        memmove(start->text + local_pos,
                start->text + local_pos + len,
                (start->len - (local_pos + len)) + 1);
        start->len -= len;
        doc->num_characters -= len;

        if (ol_damaged &&
            start->next &&
            start->next->type == ORDERED_LIST_ITEM)
        {
            renumber_list_from(start->next);
        }

        update_meta_log(doc->meta_log, snapshot_pos, len);
        return SUCCESS;
    }

    /* 2b) Spanning‐delete across chunks */
    size_t to_delete = len;
    size_t total_deleted = 0;

    /* remove tail of start */
    size_t rem = start->len - local_pos;
    to_delete -= rem;
    total_deleted += rem;

    /* free any fully‐deleted intermediate chunks */
    Chunk *curr = start->next;
    while (curr && to_delete >= curr->len)
    {
        if (curr->type == ORDERED_LIST_ITEM)
            ol_damaged = true;

        to_delete -= curr->len;
        total_deleted += curr->len;

        Chunk *tmp = curr;
        curr = curr->next;
        tmp->previous->next = curr;
        if (curr)
            curr->previous = tmp->previous;
        else
            doc->tail = tmp->previous;
        free_chunk(tmp);
        doc->num_chunks--;
    }

    /* compute suffix in curr */
    size_t suffix_len = 0;
    if (curr)
    {
        suffix_len = curr->len - to_delete;
        total_deleted += to_delete;
    }

    /* 3) Update counts */
    doc->num_characters -= total_deleted;

    /* 4) Merge or remove start chunk */
    Chunk *after_merge = NULL;
    if (local_pos + suffix_len > 0)
    {
        chunk_ensure_cap(start, local_pos + suffix_len);
        if (suffix_len)
        {
            memcpy(start->text + local_pos,
                   curr->text + to_delete,
                   suffix_len + 1);
        }
        else
        {
            start->text[local_pos] = '\0';
        }
        start->len = local_pos + suffix_len;

        if (curr)
        {
            after_merge = curr->next;
            if (curr->type == ORDERED_LIST_ITEM)
                ol_damaged = true;
            start->next = curr->next;
            if (curr->next)
                curr->next->previous = start;
            else
                doc->tail = start;
            free_chunk(curr);
            doc->num_chunks--;
        }
        else
        {
            /* no curr: everything after start remains */
            after_merge = start->next;
        }
    }
    else
    {
        /* entire start removed */
        after_merge = start->next;
        if (start->previous)
            start->previous->next = after_merge;
        else
            doc->head = after_merge;
        if (after_merge)
            after_merge->previous = start->previous;
        else
            doc->tail = start->previous;
        if (start->type == ORDERED_LIST_ITEM)
            ol_damaged = true;
        free_chunk(start);
        doc->num_chunks--;
    }

    /* 5) Post‐Process OL renumbering */
    if (ol_damaged &&
        after_merge &&
        after_merge->type == ORDERED_LIST_ITEM)
    {
        renumber_list_from(after_merge);
    }

    update_meta_log(doc->meta_log, snapshot_pos, -(int)total_deleted);
    return SUCCESS;
}

// === Formatting Commands ===
int naive_newline(document *doc, size_t pos)
{
    size_t snapshot_pos = pos;

    range *r = clamp_to_valid(doc, snapshot_pos);
    size_t effective_pos = r ? r->start : snapshot_pos;

    size_t working_pos = map_snapshot_to_working(doc->meta_log, effective_pos);

    return naive_newline_raw(doc, working_pos, snapshot_pos);
}

int naive_newline_raw(document *doc, size_t working_pos, size_t snapshot_pos)
{
    if (working_pos > doc->num_characters)
    {
        return INVALID_CURSOR_POS;
    }

    if (doc->head == NULL)
    {
        Chunk *new_chunk = (Chunk *)Calloc(1, sizeof(Chunk));
        size_t cap = calculate_cap(2);
        char *text = (char *)Calloc(cap, sizeof(char));
        text[0] = '\n';
        text[1] = '\0';
        init_chunk(new_chunk, PLAIN, 1, cap, text, 0, NULL, NULL);

        doc->head = new_chunk;
        doc->tail = new_chunk;
        doc->num_characters = 1;
        doc->num_chunks++;

        update_meta_log(doc->meta_log, snapshot_pos, 1);
        return SUCCESS;
    }

    size_t local_pos;
    Chunk *curr = locate_chunk(doc, working_pos, &local_pos);

    size_t num_remaining = curr->len - local_pos;

    Chunk *new = (Chunk *)Calloc(1, sizeof(Chunk));
    size_t cap = calculate_cap(num_remaining + 1);
    char *new_text = (char *)Calloc(cap, sizeof(char));

    memmove(new_text, curr->text + local_pos, num_remaining);
    new_text[num_remaining] = '\0';
    init_chunk(new, PLAIN, num_remaining, cap, new_text, 0, curr->next, curr);

    if (curr->next)
        curr->next->previous = new;

    if (curr == doc->tail)
        doc->tail = new;

    doc->num_characters++;
    doc->num_chunks++;

    curr->text[local_pos] = '\n';
    curr->text[local_pos + 1] = '\0';
    curr->next = new;
    curr->len = local_pos + 1;

    if (new->next && new->next->type == ORDERED_LIST_ITEM)
    {
        new->next->index_OL = 1;
        renumber_list_from(new->next);
    }

    update_meta_log(doc->meta_log, snapshot_pos, 1);
    return SUCCESS;
}

int naive_heading(document *doc, size_t level, size_t pos)
{
    if (level < 1 || level > 3 || pos > doc->snapshot_len)
    {
        return INVALID_CURSOR_POS;
    }

    // 1) Determine prefix and type
    size_t snapshot_pos = pos;

    range *r = clamp_to_valid(doc, snapshot_pos);
    size_t effective_pos = r ? r->start : snapshot_pos;

    pos = map_snapshot_to_working(doc->meta_log, effective_pos);

    const char *prefix = NULL;
    chunk_type type = PLAIN;
    if (level == 1)
    {
        prefix = "# ";
        type = HEADING1;
    }
    else if (level == 2)
    {
        prefix = "## ";
        type = HEADING2;
    }
    else
    {
        prefix = "### ";
        type = HEADING3;
    }

    size_t prefix_len = strlen(prefix);

    // 2) Empty document case
    if (doc->head == NULL)
    {
        size_t cap = calculate_cap(prefix_len + 1);
        char *text = Calloc(cap, sizeof(char));
        memcpy(text, prefix, prefix_len);
        text[prefix_len] = '\0';

        Chunk *new_chunk = Calloc(1, sizeof(Chunk));
        init_chunk(new_chunk, type, prefix_len, cap, text, 0, NULL, NULL);

        doc->head = new_chunk;
        doc->tail = new_chunk;
        doc->num_chunks = 1;
        doc->num_characters = prefix_len;

        update_meta_log(doc->meta_log, snapshot_pos, prefix_len);
        return SUCCESS;
    }

    // 3) Normalize to a one-line chunk at line start
    size_t local_pos;
    Chunk *curr = ensure_line_start(doc, &pos, &local_pos, snapshot_pos);

    // 4) Insert prefix and update type
    chunk_ensure_cap(curr, prefix_len);
    memmove(curr->text + prefix_len, curr->text, curr->len + 1);
    memcpy(curr->text, prefix, prefix_len);
    curr->len += prefix_len;
    doc->num_characters += prefix_len;
    curr->type = type;
    curr->index_OL = 0;

    update_meta_log(doc->meta_log, snapshot_pos, prefix_len);

    return SUCCESS;
}

int naive_bold(document *doc, size_t start, size_t end)
{
    if (!doc || start >= end || end > doc->snapshot_len)
        return INVALID_CURSOR_POS;

    size_t snapshot_start = start;
    size_t snapshot_end = end;

    // Determine if either start or end is in a deleted range
    range *r1 = clamp_to_valid(doc, snapshot_start);
    range *r2 = clamp_to_valid(doc, snapshot_end);

    // both in deleted region
    if (r1 && r2)
        return DELETED_POSITION;

    // Compute clamped effective positions
    size_t effective_start = (r1 && !r2) ? r1->end : snapshot_start;
    size_t effective_end = (!r1 && r2) ? r2->start : snapshot_end;

    if (effective_start >= effective_end)
        return INVALID_CURSOR_POS;

    // Map clamped positions to working document
    size_t working_start = map_snapshot_to_working(doc->meta_log, effective_start);
    size_t working_end = map_snapshot_to_working(doc->meta_log, effective_end);

    // Insert at end first, then start
    int res1 = naive_insert_raw(doc, working_end, snapshot_end, "**");
    int res2 = naive_insert_raw(doc, working_start, snapshot_start, "**");

    return (res1 == SUCCESS && res2 == SUCCESS) ? SUCCESS : INVALID_CURSOR_POS;
}

int naive_italic(document *doc, size_t start, size_t end)
{
    if (!doc || start >= end || end > doc->snapshot_len)
        return INVALID_CURSOR_POS;

    size_t snapshot_start = start;
    size_t snapshot_end = end;

    // Determine if either start or end is in a deleted range
    range *r1 = clamp_to_valid(doc, snapshot_start);
    range *r2 = clamp_to_valid(doc, snapshot_end);

    // both in deleted region
    if (r1 && r2)
        return DELETED_POSITION;

    // Compute clamped effective positions
    size_t effective_start = (r1 && !r2) ? r1->end : snapshot_start;
    size_t effective_end = (!r1 && r2) ? r2->start : snapshot_end;

    if (effective_start >= effective_end)
        return INVALID_CURSOR_POS;

    // Map clamped positions to working document
    size_t working_start = map_snapshot_to_working(doc->meta_log, effective_start);
    size_t working_end = map_snapshot_to_working(doc->meta_log, effective_end);

    // Insert at end first, then start
    int res1 = naive_insert_raw(doc, working_end, snapshot_end, "*");
    int res2 = naive_insert_raw(doc, working_start, snapshot_start, "*");

    return (res1 == SUCCESS && res2 == SUCCESS) ? SUCCESS : INVALID_CURSOR_POS;
}

int naive_blockquote(document *doc, size_t pos)
{
    size_t snapshot_pos = pos;

    range *r = clamp_to_valid(doc, snapshot_pos);
    size_t effective_pos = r ? r->start : snapshot_pos;

    pos = map_snapshot_to_working(doc->meta_log, effective_pos);

    // 1) prefix
    const char *prefix = "> ";
    chunk_type type = BLOCKQUOTE;
    size_t prefix_len = 2;

    // 2) Empty document case
    if (doc->head == NULL)
    {
        size_t cap = calculate_cap(prefix_len + 1);
        char *text = Calloc(cap, sizeof(char));
        memcpy(text, prefix, prefix_len);
        text[prefix_len] = '\0';

        Chunk *new_chunk = Calloc(1, sizeof(Chunk));
        init_chunk(new_chunk, type, prefix_len, cap, text, 0, NULL, NULL);

        doc->head = new_chunk;
        doc->tail = new_chunk;
        doc->num_chunks = 1;
        doc->num_characters = prefix_len;
        update_meta_log(doc->meta_log, snapshot_pos, prefix_len);
        return SUCCESS;
    }

    // 3) Normalize to a one-line chunk at line start
    size_t local_pos;
    Chunk *curr = ensure_line_start(doc, &pos, &local_pos, snapshot_pos);

    // 4) Insert prefix and update type
    chunk_ensure_cap(curr, prefix_len);
    memmove(curr->text + prefix_len, curr->text, curr->len + 1);
    memcpy(curr->text, prefix, prefix_len);
    curr->len += prefix_len;
    doc->num_characters += prefix_len;

    curr->type = type;
    curr->index_OL = 0;

    update_meta_log(doc->meta_log, snapshot_pos, prefix_len);

    return SUCCESS;
}

int naive_ordered_list(document *doc, size_t pos)
{
    if (pos > doc->snapshot_len)
    {
        return INVALID_CURSOR_POS;
    }

    size_t snapshot_pos = pos;
    range *r = clamp_to_valid(doc, snapshot_pos);
    size_t effective_pos = r ? r->start : snapshot_pos;

    pos = map_snapshot_to_working(doc->meta_log, effective_pos);

    if (doc->head == NULL)
    {

        if (pos != 0)
            return INVALID_CURSOR_POS;

        // Create a new chunk with "1. " as its entire line
        const char *text = "1. ";
        size_t len = 3;
        size_t cap = calculate_cap(len + 1);
        char *buf = Calloc(cap, 1);
        memcpy(buf, text, len);
        buf[len] = '\0';

        Chunk *c = Calloc(1, sizeof(Chunk));
        init_chunk(c, ORDERED_LIST_ITEM,
                   len, cap, buf,
                   1, // index
                   NULL, NULL);

        doc->head = c;
        doc->tail = c;
        doc->num_chunks = 1;
        doc->num_characters = len;

        update_meta_log(doc->meta_log, snapshot_pos, len);
        return SUCCESS;
    }

    // 1) Normalize into a single-line chunk at the start of that line
    size_t local_pos;
    Chunk *curr = ensure_line_start(doc, &pos, &local_pos, snapshot_pos);

    // 2) Compute list index from previous list item
    int base = prev_ol_index(curr);
    int my_index = base + 1;
    if (my_index > 9)
    {
        my_index = 9;
    }

    // 3) Build and insert the prefix "N. "
    const size_t prefix_len = 3;
    char prefix[4] = {
        (char)('0' + my_index),
        '.',
        ' ',
        '\0'};

    chunk_ensure_cap(curr, prefix_len);
    memmove(curr->text + prefix_len,
            curr->text,
            curr->len + 1); // include '\0'
    memcpy(curr->text, prefix, prefix_len);
    curr->len += prefix_len;
    doc->num_characters += prefix_len;

    // 4) Update metadata and renumber the rest
    curr->type = ORDERED_LIST_ITEM;
    curr->index_OL = my_index;
    renumber_list_from(curr);

    update_meta_log(doc->meta_log, snapshot_pos, prefix_len);
    return SUCCESS;
}

int naive_unordered_list(document *doc, size_t pos)
{
    size_t snapshot_pos = pos;
    range *r = clamp_to_valid(doc, snapshot_pos);
    size_t effective_pos = r ? r->start : snapshot_pos;

    pos = map_snapshot_to_working(doc->meta_log, effective_pos);

    // 1) prefix
    const char *prefix = "- ";
    chunk_type type = UNORDERED_LIST_ITEM;
    size_t prefix_len = 2;

    // 2) Empty document case
    if (doc->head == NULL)
    {
        size_t cap = calculate_cap(prefix_len + 1);
        char *text = Calloc(cap, sizeof(char));
        memcpy(text, prefix, prefix_len);
        text[prefix_len] = '\0';

        Chunk *new_chunk = Calloc(1, sizeof(Chunk));
        init_chunk(new_chunk, type, prefix_len, cap, text, 0, NULL, NULL);

        doc->head = new_chunk;
        doc->tail = new_chunk;
        doc->num_chunks = 1;
        doc->num_characters = prefix_len;

        update_meta_log(doc->meta_log, snapshot_pos, prefix_len);

        return SUCCESS;
    }
    // 3) Normalize to a one-line chunk at line start
    size_t local_pos;
    Chunk *curr = ensure_line_start(doc, &pos, &local_pos, snapshot_pos);

    // 4) Insert prefix and update type
    chunk_ensure_cap(curr, prefix_len);
    memmove(curr->text + prefix_len, curr->text, curr->len + 1);
    memcpy(curr->text, prefix, prefix_len);
    curr->len += prefix_len;
    doc->num_characters += prefix_len;

    curr->type = type;
    curr->index_OL = 0;

    update_meta_log(doc->meta_log, snapshot_pos, prefix_len);

    return SUCCESS;
}

int naive_code(document *doc, size_t start, size_t end)
{
    if (!doc || start >= end || end > doc->snapshot_len)
        return INVALID_CURSOR_POS;

    size_t snapshot_start = start;
    size_t snapshot_end = end;

    // Determine if either start or end is in a deleted range
    range *r1 = clamp_to_valid(doc, snapshot_start);
    range *r2 = clamp_to_valid(doc, snapshot_end);

    // both in deleted region
    if (r1 && r2)
        return DELETED_POSITION;

    // Compute clamped effective positions
    size_t effective_start = (r1 && !r2) ? r1->end : snapshot_start;
    size_t effective_end = (!r1 && r2) ? r2->start : snapshot_end;

    if (effective_start >= effective_end)
        return INVALID_CURSOR_POS;

    // Map clamped positions to working document
    size_t working_start = map_snapshot_to_working(doc->meta_log, effective_start);
    size_t working_end = map_snapshot_to_working(doc->meta_log, effective_end);

    int res1 = naive_insert_raw(doc, working_end, snapshot_end, "`");
    int res2 = naive_insert_raw(doc, working_start, snapshot_start, "`");

    return (res1 == SUCCESS && res2 == SUCCESS) ? SUCCESS : INVALID_CURSOR_POS;
}

int naive_horizontal_rule(document *doc, size_t pos)
{
    if (pos > doc->snapshot_len)
    {
        return INVALID_CURSOR_POS;
    }

    size_t snapshot_pos = pos;
    range *r = clamp_to_valid(doc, snapshot_pos);
    size_t effective_pos = r ? r->start : snapshot_pos;

    pos = map_snapshot_to_working(doc->meta_log, effective_pos);

    if (doc->head == NULL)
    {
        const char *hr_text = "---\n";
        size_t len = 4;
        size_t cap = calculate_cap(len + 1);
        char *text = Calloc(cap, sizeof(char));
        memcpy(text, hr_text, len);
        text[len] = '\0';

        Chunk *c = Calloc(1, sizeof(Chunk));
        init_chunk(c, HORIZONTAL_RULE, len, cap, text, 0, NULL, NULL);

        doc->head = c;
        doc->tail = c;
        doc->num_chunks = 1;
        doc->num_characters = len;

        update_meta_log(doc->meta_log, snapshot_pos, len);
        return SUCCESS;
    }

    // 1) Locate & split to line start
    size_t local;
    Chunk *curr = ensure_line_start(doc, &pos, &local, snapshot_pos);
    // Now `curr` begins exactly at pos, at the start of a line.

    // 2) Create a standalone HR chunk
    const char *hr_text = "---\n";
    size_t hr_len = 4;
    size_t cap = calculate_cap(hr_len + 1);
    char *buf = Calloc(cap, 1);
    memcpy(buf, hr_text, hr_len);
    buf[hr_len] = '\0';

    Chunk *hr = Calloc(1, sizeof(Chunk));
    init_chunk(hr,
               HORIZONTAL_RULE,
               hr_len,
               cap,
               buf,
               0,
               curr,
               curr->previous);

    // Splice it in front of `curr`
    if (curr->previous)
        curr->previous->next = hr;
    else
        doc->head = hr;

    curr->previous = hr;

    doc->num_chunks++;
    doc->num_characters += hr_len;

    update_meta_log(doc->meta_log, snapshot_pos, hr_len);

    return SUCCESS;
}

int naive_link(document *doc, size_t start, size_t end, const char *url)
{
    if (!doc || start >= end || end > doc->snapshot_len || url == NULL)
        return INVALID_CURSOR_POS;

    size_t snapshot_start = start;
    size_t snapshot_end = end;

    
    range *r1 = clamp_to_valid(doc, snapshot_start);
    range *r2 = clamp_to_valid(doc, snapshot_end);

    if (r1 && r2)
        return DELETED_POSITION;

    // Clamping
    size_t effective_start = (r1 && !r2) ? r1->end : snapshot_start;
    size_t effective_end   = (!r1 && r2) ? r2->start : snapshot_end;

    if (effective_start >= effective_end)
        return INVALID_CURSOR_POS;

    //Mapping
    size_t working_start = map_snapshot_to_working(doc->meta_log, effective_start);
    size_t working_end   = map_snapshot_to_working(doc->meta_log, effective_end);

    // Construct full suffix: "](" + url + ")"
    size_t url_len = strlen(url);
    size_t suffix_len = 3 + url_len + 1; // "](", url, ')', '\0'
    char *suffix = Malloc(suffix_len);
    snprintf(suffix, suffix_len, "](%s)", url);

    // 5) Insert in reverse order
    int res1 = naive_insert_raw(doc, working_end, snapshot_end, suffix);
    int res2 = naive_insert_raw(doc, working_start, snapshot_start, "[");

    free(suffix);

    return (res1 == SUCCESS && res2 == SUCCESS) ? SUCCESS : INVALID_CURSOR_POS;
}

