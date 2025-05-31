#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>


/**
 * This file is the header file for all the document functions. You will be tested on the functions inside markdown.h
 * You are allowed to and encouraged multiple helper functions and data structures, and make your code as modular as possible.
 * Ensure you DO NOT change the name of document struct.
 */

typedef enum
{
    PLAIN,
    HEADING1,
    HEADING2,
    HEADING3,
    BLOCKQUOTE,
    UNORDERED_LIST_ITEM,
    ORDERED_LIST_ITEM,
    HORIZONTAL_RULE

} chunk_type;

typedef struct chunk
{
    chunk_type type;
                          
    size_t len;   // number of bytes in `text` (exclude null byte)
    size_t cap;   // max. capacity of chunk, (multiples of 128)

    char *text;   // malloc-owned buffer, null-terminated
    int index_OL; // valid only if type == ORDERED_LIST_ITEM
    
    struct chunk *next;
    struct chunk *previous;
} Chunk;

typedef struct
{
    Chunk *head;
    Chunk *tail;

    size_t num_chunks;     // total number of lines (chunks)
    size_t num_characters; // total character count 

    uint64_t version;      // incremented during TIME_INTERVAL broadcast

} document;

// Functions from here onwards.

// === Document helpers ===
Chunk* locate_chunk(document* doc, size_t pos, size_t* local_pos);
void split_and_format_chunk(document *doc, Chunk *curr, size_t local_pos,
                            const char *prefix, size_t prefix_len, chunk_type new_type);

Chunk *ensure_line_start(document *doc,
                                size_t *pos_out,
                                size_t *local_pos_out);

// === Chunk helpers ===
void init_chunk(Chunk *chunk, chunk_type type, size_t len, size_t cap, char *text, int index_OL, Chunk *next, Chunk *previous);
void free_chunk(Chunk *chunk);
size_t calculate_cap(size_t content_size);
void chunk_ensure_cap(Chunk *curr, size_t extra_content);
void chunk_insert(Chunk *curr, size_t local_pos, const char *content, size_t content_size);
void renumber_list_from(Chunk *start);
int prev_ol_index(Chunk *chunk);

#endif 
