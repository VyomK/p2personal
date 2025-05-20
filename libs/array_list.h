#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <stddef.h> // for size_t


typedef struct dynamic_array {
    void **data;       // Pointer to array of elements
    size_t size;       // Number of elements currently in the array
    size_t capacity;   // Allocated capacity
} array_list;


array_list *create_array(size_t capacity);

void append_to(array_list *array, void *element);

void *get_from(array_list *array, int pos);

void *remove_at(array_list *array, int pos);

void *remove_from(array_list *array, void *element);

void free_array(array_list *array, int free_items);

#endif
