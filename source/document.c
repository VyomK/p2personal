#include "document.h"
#include "markdown.h"
#include "memory.h"
#include "naive_ops.h"



void update_meta_log(array_list *meta_positions, size_t snapshot_pos, int offset) {
    meta_pos *m = Calloc(1, sizeof(meta_pos));
    m->snapshot_pos = snapshot_pos;
    m->offset = offset;
    append_to(meta_positions, m);
}

range *clamp_to_valid(document *doc, size_t pos) {
    if (!doc || pos > doc->snapshot_len) {
        return NULL;
    }

    for (size_t i = 0; i < doc->deleted_ranges->size; ++i) {
        range *r = (range *)get_from(doc->deleted_ranges, i);
        if (pos >= r->start && pos < r->end) {
            return r; 
        }
    }

    return NULL; 
}

size_t map_snapshot_to_working(array_list *meta_log, size_t clamped_snapshot_pos) {
    long offset = 0;

    for (size_t i = 0; i < meta_log->size; ++i) {
        meta_pos *m = (meta_pos *)get_from(meta_log, i);

        if (m->snapshot_pos < clamped_snapshot_pos) {
            offset += m->offset;
        }
    }

    long result = (long) clamped_snapshot_pos + offset;
    return (result < 0) ? 0 : (size_t)result;
}


// === NAIVE DOC STRUCTURE HELPERS ===
// === Document helpers ===

Chunk *locate_chunk(document *doc, size_t pos, size_t *local_pos)
{
    Chunk *curr = doc->head;
    size_t curr_document_pos = 0;
    while (curr && curr_document_pos + curr->len <= pos)
    {
        curr_document_pos += curr->len;
        curr = curr->next;
    }

    if (!curr && pos == doc->num_characters && doc->tail)
    {
        *local_pos = doc->tail->len;
        return doc->tail;
    }

    if (!curr)
        return NULL;

    *local_pos = pos - curr_document_pos;
    return curr;
}

void split_and_format_chunk(document *doc, Chunk *curr, size_t local_pos,
                            const char *prefix, size_t prefix_len, chunk_type new_type)
{
    // Copy tail content from curr into new chunk
    size_t tail_len = curr->len - local_pos;
    size_t new_len = prefix_len + tail_len;
    size_t new_cap = calculate_cap(new_len + 1);

    char *new_text = Calloc(new_cap, sizeof(char));
    memcpy(new_text, prefix, prefix_len);
    memcpy(new_text + prefix_len, curr->text + local_pos, tail_len);
    new_text[new_len] = '\0';

    Chunk *new_chunk = Calloc(1, sizeof(Chunk));
    init_chunk(new_chunk, new_type, new_len, new_cap, new_text, 0, curr->next, curr);

    if (curr->next)
    {
        curr->next->previous = new_chunk;
    }
    else
    {
        doc->tail = new_chunk;
    }

    curr->next = new_chunk;

    // Truncate current chunk
    curr->len = local_pos + 1;
    curr->text[local_pos] = '\n';
    curr->text[local_pos + 1] = '\0';

    doc->num_chunks++;
    doc->num_characters += prefix_len + 1; // +1 for inserted '\n'

    if (new_chunk->previous && new_chunk->previous->type == ORDERED_LIST_ITEM &&
        new_chunk->next && new_chunk->next->type == ORDERED_LIST_ITEM)
    {
        new_chunk->next->index_OL = 1;
        renumber_list_from(new_chunk->next);
    }
}


Chunk *ensure_line_start(document *doc, size_t *pos_out, size_t *local_pos_out, size_t snapshot_pos)
{
    
    size_t local;
    Chunk *curr = locate_chunk(doc, *pos_out, &local);

    // If we're in the middle of a line, split it:
    if (local > 0) {
        // Inserts a '\n' at pos and creates a new chunk for the rest
        naive_newline_raw(doc, *pos_out, snapshot_pos);
        (*pos_out) += 1;           // move past the inserted newline
        curr = curr->next;         // now the first chunk of the new line
        local = 0;                 
    }

    *local_pos_out = local;
    return curr;
}
// === Chunk helpers ===
void init_chunk(Chunk *chunk, chunk_type type, size_t len, size_t cap, char *text, int index_OL, Chunk *next, Chunk *previous)
{

    chunk->type = type;
    chunk->len = len;
    chunk->cap = cap;
    chunk->text = text;
    chunk->index_OL = index_OL;
    chunk->next = next;
    chunk->previous = previous;
    return;
}

void free_chunk(Chunk *chunk)
{
    if (chunk)
    {
        free(chunk->text);
        free(chunk);
    }
}

size_t calculate_cap(size_t content_size)
{
    size_t cap = 128;
    while (content_size > cap)
    {
        cap *= 2;
    }
    return cap;
}

void chunk_ensure_cap(Chunk *curr, size_t extra_content)
{
    if (curr->len + extra_content + 1 <= curr->cap)
        return;

    size_t new_cap = calculate_cap(curr->len + extra_content + 1);
    curr->text = Realloc(curr->text, new_cap);
    curr->cap = new_cap;
}

void chunk_insert(Chunk *curr, size_t local_pos, const char *content, size_t content_size)
{
    chunk_ensure_cap(curr, content_size);

    memmove(curr->text + local_pos + content_size,
            curr->text + local_pos,
            curr->len - local_pos + 1); // includes '\0'

    memcpy(curr->text + local_pos, content, content_size);

    curr->len += content_size;
}

int prev_ol_index(Chunk *c)
{
    if (c && c->previous && c->previous->type == ORDERED_LIST_ITEM){
        return c->previous->index_OL;
    }
        
    return 0;
}

void renumber_list_from(Chunk *start)
{
    int idx = prev_ol_index(start);
    for (Chunk *q = start; q && q->type == ORDERED_LIST_ITEM; q = q->next)
    {
        if (idx < 9)
            idx++;
        q->index_OL = idx;
        if (q->len >= 3)
        {
            q->text[0] = '0' + idx;
            q->text[1] = '.';
            q->text[2] = ' ';
        }
    }
}