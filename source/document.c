#include "document.h"
#include "memory.h"

// === Document helpers ===
document *init_doc(document *doc, Chunk *head, Chunk *tail, size_t num_chunks, size_t num_characters, uint64_t version)
{

    doc->head = head;
    doc->tail = tail;
    doc->num_chunks = num_chunks;
    doc->num_characters = num_characters;
    doc->version = version;
    return doc;
}

// === Chunk helpers ===
Chunk *init_chunk(Chunk *chunk,
                  chunk_type type,
                  size_t len,
                  size_t cap,
                  char *text, int index_OL,
                  Chunk *next,
                  Chunk *previous)
{

    chunk->type = type;
    chunk->len = len;
    chunk->cap = cap;
    chunk->text = text;
    chunk->index_OL = index_OL;
    chunk->next = next;
    chunk->previous = previous;
    return chunk;
}

void free_chunk(Chunk *chunk)
{
    if (chunk)
    {
        free(chunk->text);
        free(chunk);
    }
}

