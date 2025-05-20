#include "array_list.h"
#include <stdlib.h>
#include <stdio.h>

array_list *create_array(size_t capacity)
{
    array_list *arr = calloc(1, sizeof(array_list));
    arr->data = calloc(capacity, sizeof(void *));
    arr->size = 0;
    arr->capacity = capacity;
    return arr;
}

void append_to(array_list *array, void *element)
{
    if (array->size >= array->capacity) {
        array->capacity *= 2;
        array->data = realloc(array->data, array->capacity * sizeof(void *));
    }
    array->data[array->size] = element;
    array->size++;
}

void *get_from(array_list *array, int pos)
{
    if (pos < 0 || pos >= (int)array->size) {
        return NULL;
    }
    return array->data[pos];
}

void *remove_at(array_list *array, int pos)
{
    if (pos < 0 || pos >= (int)array->size) {
        return NULL;
    }

    void *element = array->data[pos];
    for (int i = pos; i < (int)array->size - 1; i++) {
        array->data[i] = array->data[i + 1];
    }
    array->size--;
    return element;
}

void *remove_from(array_list *array, void *element)
{
    for (int i = 0; i < (int)array->size; i++) {
        if (array->data[i] == element) {
            return remove_at(array, i);
        }
    }
    return NULL;
}

void free_array(array_list *array, int free_items)
{
    if (free_items) {
        for (size_t i = 0; i < array->size; i++) {
            free(array->data[i]);
        }
    }
    free(array->data);
    free(array);
}

