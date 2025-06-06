#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <stddef.h>



typedef struct dynamic_array {
    void **data;       
    size_t size;       
    size_t capacity;   
} array_list;


array_list *create_array(size_t capacity);

void append_to(array_list *array, void *element);

void *get_from(array_list *array, int pos);

void *remove_at(array_list *array, int pos);

void *remove_from(array_list *array, void *element);

void free_array(array_list *array);

array_list* clear_array(array_list *array);

#endif
