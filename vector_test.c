#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vector.h"
#include <assert.h>

static void free_string(void *string);

int main(int argc, char const *argv[])
{
  int i;
  char *temp_string;

  Vector *v = vector_init(&free_string);
  for (i = 0; i < 1123; i++) {
    temp_string = malloc(sizeof(char) * 80);
    sprintf(temp_string, "hello%d", i);
    vector_append(v, temp_string);
  }
  assert(vector_size(v) == 1123);
  assert(strcmp(vector_get(v, 100), "hello100") == 0);
  assert(strcmp(vector_get(v, 1122), "hello1122") == 0);
  vector_remove(v, 2);
  vector_remove(v, 3);
  // 1 2 3 4
  // 1 3 4 5
  // 1 3 5
  assert(strcmp(vector_get(v, 1), "hello1") == 0);
  assert(strcmp(vector_get(v, 2), "hello3") == 0);
  assert(strcmp(vector_get(v, 3), "hello5") == 0);
  assert(vector_size(v) == 1121);

  vector_free(v);

  return 0;
}

static void free_string(void *string) {
  free(string);
}

