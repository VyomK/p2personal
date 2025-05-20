#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void *Malloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr)
    {
        fprintf(stderr, "Malloc failed (%zu bytes) [%s:%d]\n", size, __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *Calloc(size_t nmemb, size_t size)
{
    void *ptr = calloc(nmemb, size);
    if (!ptr)
    {
        fprintf(stderr, "Calloc failed (%zu x %zu bytes) [%s:%d]\n", nmemb, size, __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *Realloc(void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr)
    {
        fprintf(stderr, "Realloc failed (%zu bytes) [%s:%d]\n", size, __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

void *ReCalloc(void *ptr, size_t old_size, size_t new_size)
{
    void *new_ptr = realloc(ptr, new_size);
    if (!new_ptr)
    {
        fprintf(stderr, "ReCalloc failed (%zu → %zu bytes) [%s:%d]\n", old_size, new_size, __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }

    if (new_size > old_size)
    {
        
        memset((char *)new_ptr + old_size, 0, new_size - old_size);
    }

    return new_ptr;
}


