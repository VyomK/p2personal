#ifdef MEMORY_WRAPPERS
#define MEMORY_WRAPPERS

#include <stdlib.h>

void *Calloc(size_t nmemb, size_t size);
void *Malloc(size_t size);
void *Realloc(void *ptr, size_t size);
char *Strdup(const char *s);

#endif