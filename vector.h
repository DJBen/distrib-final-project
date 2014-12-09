// vector.h

#ifndef _VECTOR_H_
#define _VECTOR_H_

#include <stdbool.h>

#define VECTOR_INITIAL_CAPACITY 100
typedef void (*vector_free_fn) (void *data);

// Define a vector type
typedef struct {
  int size;      // slots used so far
  int capacity;  // total available slots
  void **data;     // array of integers we're storing
  vector_free_fn free_fn;
} Vector;


Vector *vector_init(vector_free_fn free_fn);

void vector_append(Vector *vector, void *data);

void *vector_get(Vector *vector, int index);

void vector_set(Vector *vector, int index, void *data);

void vector_insert(Vector *vector, int index, void *data);

void vector_remove(Vector *vector, int index);

int vector_size(Vector *vector);

void vector_free(Vector *vector);

void vector_print(Vector *vector);

int vector_index_of_str(Vector *vector, char *str);

// A very inefficient map implemented by double arrays
typedef struct {
  Vector *keys;
  Vector *values;
} Map;

Map *map_init();

void map_set(Map *map, int key, int value);

bool map_has_key(Map *map, int key);

int map_get(Map *map, int key);

void map_remove(Map *map, int key);

void map_free(Map *map);

#endif

