#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "array_list.h"



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
                          
    size_t len;   
    size_t cap;   // max. capacity of chunk, (multiples of 128)

    char *text;   
    int index_OL; // valid only if type == ORDERED_LIST_ITEM
    
    struct chunk *next;
    struct chunk *previous;
} Chunk;


typedef struct meta_pos
{
    size_t snapshot_pos;
    int offset;

} meta_pos;

typedef struct range{
    size_t start; // inclusive
    size_t end; //exclusive
}range;

typedef enum {
    CMD_INSERT,
    CMD_NEWLINE,
    CMD_BLOCK_BLOCKQUOTE,
    CMD_BLOCK_HEADING,
    CMD_BLOCK_HRULE,
    CMD_BLOCK_UL_ITEM,
    CMD_BLOCK_OL_ITEM,
    CMD_INLINE_BOLD,
    CMD_INLINE_ITALIC,
    CMD_INLINE_CODE,
    CMD_INLINE_LINK
} cmd_type;

typedef struct {
    cmd_type type;

    size_t snap_pos;     // for insertions and block commands
    size_t end_pos;      // only for inline: snap_pos (== start_pos)
    
    const char *content; // only for CMD_INSERT and CMD_INLINE_LINK (text or URL)

    size_t heading_level; // only for CMD_BLOCK_HEADING
} cmd;


typedef struct
{
    Chunk *head;
    Chunk *tail;

    size_t num_chunks;     // total number of lines (chunks)
    size_t num_characters; // total character count  
        
    char* snapshot;
    size_t snapshot_len;

    array_list* meta_log;
    array_list* deleted_ranges;
    array_list* cmd_list;


} document;


void update_meta_log(array_list *meta_positions, size_t snapshot_pos, int offset);
range *clamp_to_valid(document *doc, size_t pos);
size_t map_snapshot_to_working(array_list *meta_log, size_t clamped_snapshot_pos);

// === NAIVE DOC STRUCTURE HELPERS ===
// === Document helpers ===
Chunk* locate_chunk(document* doc, size_t pos, size_t* local_pos);

Chunk *ensure_line_start(document *doc, size_t *pos_out, size_t *local_pos_out, size_t snapshot_pos);

// === Chunk helpers ===
void init_chunk(Chunk *chunk, chunk_type type, size_t len, size_t cap, char *text, int index_OL, Chunk *next, Chunk *previous);
void free_chunk(Chunk *chunk);
size_t calculate_cap(size_t content_size);
void chunk_ensure_cap(Chunk *curr, size_t extra_content);
void chunk_insert(Chunk *curr, size_t local_pos, const char *content, size_t content_size);
void renumber_list_from(Chunk *start);
int prev_ol_index(Chunk *chunk);

#endif 
