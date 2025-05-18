#ifndef DOCUMENT_H

#define DOCUMENT_H
/**
 * This file is the header file for all the document functions. You will be tested on the functions inside markdown.h
 * You are allowed to and encouraged multiple helper functions and data structures, and make your code as modular as possible. 
 * Ensure you DO NOT change the name of document struct.
 */


typedef enum{   
    // covers only block-types, inline types will edit the text directly
    PLAIN,
    HEADING1,
    HEADING2, 
    HEADING3, 
    BLOCKQUOTE,
    UNORDERED_LIST_ITEM,
    ORDERED_LIST_ITEM,
    HORIZONTAL_RULE

} chunk_type;

typedef struct chunk{

    chunk_type type;
    size_t len;
    char* text;
    int index_OL; // only for type == ORDERED_LIST_ITEM (1-9), set as 0 for others
    struct chunk* next;
    struct chunk* previous;

}Chunk;

typedef struct {
    Chunk* head;
    Chunk* tail;

    size_t lines; 
    uint64_t version;

} document;

// Functions from here onwards.
#endif
