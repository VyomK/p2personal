#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>



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
        
    }
    return new_ptr;
}

