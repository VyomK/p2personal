#ifndef MEMORY_WRAPPERS
#define MEMORY_WRAPPERS

#include <stdlib.h>

void *Calloc(size_t nmemb, size_t size);
void *Malloc(size_t size);
void *Realloc(void *ptr, size_t size);
void *ReCalloc(void *ptr, size_t old_size, size_t new_size);
char *Strdup(const char *s);

#endif