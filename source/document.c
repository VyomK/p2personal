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

char *flatten_document(document *doc) {
    if (!doc || !doc->head)
        return Calloc(1,sizeof(char));  

    
    size_t total = doc->num_characters;
    char *buf = Calloc(total + 1, sizeof(char)); // +1 for '\0'
    char *p = buf;

    Chunk *curr = doc->head;
    while (curr) {
        memcpy(p, curr->text, curr->len);
        p += curr->len;
        curr = curr->next;
    }

    *p = '\0'; 
    return buf;
}


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