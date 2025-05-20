#include "document.h"
#include "memory.h"

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

    *local_pos = pos - curr_document_pos;
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
