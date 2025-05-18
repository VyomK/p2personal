#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <stddef.h> // for size_t


typedef struct {
    void **data;       // Pointer to array of elements
    size_t size;       // Number of elements currently in the array
    size_t capacity;   // Allocated capacity
} ArrayList;


ArrayList *create_array(size_t capacity);

void append_to(ArrayList *array, void *element);

void *get_from(ArrayList *array, int pos);

void *remove_at(ArrayList *array, int pos);

void *remove_from(ArrayList *array, void *element);

void free_array(ArrayList *array, int free_items);

#endif
