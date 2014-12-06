// vector.h

#define VECTOR_INITIAL_CAPACITY 100

// Define a vector type
typedef struct {
  int size;      // slots used so far
  int capacity;  // total available slots
  char **data;     // array of integers we're storing
} Vector;

void vector_init(Vector *vector);

void vector_append(Vector *vector, char *data);

char *vector_get(Vector *vector, int index);

void vector_set(Vector *vector, int index, char *data);

void vector_free(Vector *vector);
