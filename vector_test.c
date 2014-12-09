#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vector.h"
#include <assert.h>

static void free_string(void *string);
static void free_line(void *line);

typedef struct
{
  char content[120];
  char sender[36];
  int like_count;
  // All users who have liked this.
  struct vector *likes;
} FakeLine;

int main(int argc, char const *argv[])
{
  int i;
  char *temp_string;
  Map *map = map_init();

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

  printf("Append and removal test complete.\n");

  Vector *v2 = vector_init(&free_string);
  temp_string = malloc(sizeof(char) * 80);
  sprintf(temp_string, "4");
  vector_insert(v2, 0, temp_string);
  temp_string = malloc(sizeof(char) * 80);
  sprintf(temp_string, "3");
  vector_insert(v2, 0, temp_string);
  temp_string = malloc(sizeof(char) * 80);
  sprintf(temp_string, "1");
  vector_insert(v2, 0, temp_string);
  temp_string = malloc(sizeof(char) * 80);
  sprintf(temp_string, "5");
  vector_insert(v2, 3, temp_string);
  temp_string = malloc(sizeof(char) * 80);
  sprintf(temp_string, "2");
  vector_insert(v2, 1, temp_string);
  temp_string = malloc(sizeof(char) * 80);
  for (i = 0; i < 5; i++) {
    sprintf(temp_string, "%d", i + 1);
    assert(strcmp(vector_get(v2, i), temp_string) == 0);
  }
  free(temp_string);

  for (i = 1000; i >= 6; i--) {
    temp_string = malloc(sizeof(char) * 80);
    sprintf(temp_string, "%d", i);
    vector_insert(v2, 5, temp_string);
    // printf("Inserting %d\n", i);
  }

  printf("Insertion first stage test complete. Vector size = %d\n", vector_size(v2));

  temp_string = malloc(sizeof(char) * 80);
  for (i = 0; i < 1000; i++) {
    sprintf(temp_string, "%d", i + 1);
    assert(strcmp(vector_get(v2, i), temp_string) == 0);
    // printf("Assertion %s no prob.\n", (char *)vector_get(v2, i));
  }
  free(temp_string);

  vector_free(v2);

  printf("Insertion validation test complete.\n");

  for (i = 0; i < 100; i++) {
    map_set(map, 99 - i, i);
  }

  for (i = 0; i < 100; i++) {
    assert(map_get(map, i) == 99 - i);
  }

  map_set(map, 32767, 1123);
  map_set(map, 1123, 4567);

  map_remove(map, 32767);
  map_remove(map, 99);
  map_remove(map, 6);

  assert(map_get(map, 32767) == -1);
  assert(map_get(map, 99) == -1);
  assert(map_get(map, 6) == -1);
  assert(map_get(map, 7) != -1);

  map_free(map);

  printf("Map test complete.\n");

  return 0;
}

static void free_string(void *string) {
  // printf("Free being called. String to free: %s\n", (char *)string);
  free(string);
}

static void free_line(void *line) {

}

