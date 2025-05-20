#include "document.h"
#include "memory.h"

// === Document helpers ===

Chunk* locate_chunk(document* doc, size_t pos, size_t* local_pos){
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
    while (content_size > cap){
        cap *= 2;
    } 
    return cap;
} 