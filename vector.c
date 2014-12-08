// vector.c

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "vector.h"

static void vector_double_capacity_if_full(Vector *vector);

Vector *vector_init(vector_free_fn free_fn) {
  Vector *vector;
  vector = (Vector *)calloc(1, sizeof(Vector));
  if(!vector) return NULL;
  assert(free_fn != NULL);
  // initialize size and capacity
  vector->size = 0;
  vector->capacity = VECTOR_INITIAL_CAPACITY;
  vector->free_fn = free_fn;

  // allocate memory for vector->data
  vector->data = calloc(1, sizeof(void *) * vector->capacity);
  if (!vector->data) {
    free(vector);
    return NULL;
  }

  return vector;
}

void vector_append(Vector *vector, void *data) {
  assert(vector != NULL);
  // make sure there's room to expand into
  vector_double_capacity_if_full(vector);

  // append the value and increment vector->size
  vector->data[vector->size++] = data;
}

void *vector_get(Vector *vector, int index) {
  assert(vector != NULL);
  if (index >= vector->size || index < 0) {
    printf("Index %d out of bounds for vector of size %d\n", index, vector->size);
    exit(1);
  }
  return vector->data[index];
}

void vector_remove(Vector *vector, int index) {
  int i;
  assert(vector != NULL);
  if (index >= vector->size || index < 0) {
    printf("Index %d out of bounds for vector of size %d\n", index, vector->size);
    exit(1);
  }
  vector->free_fn(vector->data[index]);
  for (i = index; i < vector->size - 1; i++) {
    vector->data[i] = vector->data[i + 1];
  }
  vector->data[vector->size - 1] = NULL;
  vector->size--;
}

int vector_size(Vector *vector) {
  return vector->size;
}

void vector_set(Vector *vector, int index, void *data) {
  if (index >= vector->size || index < 0) {
    printf("Index %d out of bounds for vector of size %d\n", index, vector->size);
    exit(1);
  }
  if (data == NULL) {
    printf("Data must not be null.\n");
    exit(1);
  }

  // zero fill the vector up to the desired index
  // while (index >= vector->size) {
  //   vector_append(vector, 0);
  // }

  // set the value at the desired index
  vector->free_fn(vector->data[index]);
  vector->data[index] = data;
}

static void vector_double_capacity_if_full(Vector *vector) {
  if (vector->size >= vector->capacity) {
    // double vector->capacity and resize the allocated memory accordingly
    vector->capacity *= 2;
    vector->data = realloc(vector->data, sizeof(void *) * vector->capacity);
  }
}

void vector_free(Vector *vector) {
  int i;
  for(i = 0; i < vector->size; i++)
    if(vector->data[i]) vector->free_fn(vector->data[i]);
  free(vector->data);
  free(vector);
}

void vector_print(Vector *vector) {
  int i;
  for (i = 0; i < vector->size; i++) {
    printf("%s", (char *)vector_get(vector, i));
    if (i + 1 < vector->size) {
      printf(", ");
    }
  }
  printf("\n");
}

int vector_index_of_str(Vector *vector, char *str) {
  int i;
  for (i = 0; i < vector_size(vector); i++) {
    if (strcmp(vector_get(vector, i), str) == 0) {
      return i;
    }
  }
  return -1;
}

