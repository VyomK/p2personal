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


