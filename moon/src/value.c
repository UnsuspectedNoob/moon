#include <stdio.h>
#include <stdlib.h>

#include "object.h"
#include "value.h"

void initValueArray(ValueArray *array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void writeValueArray(ValueArray *array, Value value) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = (oldCapacity < 8) ? 8 : oldCapacity * 2;
    // We use standard realloc for now, simplified
    array->values =
        (Value *)realloc(array->values, sizeof(Value) * array->capacity);
  }

  array->values[array->count] = value;
  array->count++;
}

void freeValueArray(ValueArray *array) {
  free(array->values);
  initValueArray(array);
}

void printValue(Value value) {
  if (IS_BOOL(value)) {
    printf(AS_BOOL(value) ? "true" : "false");
  } else if (IS_NIL(value)) {
    printf("nil");
  } else if (IS_NUMBER(value)) {
    printf("%.14g", AS_NUMBER(value));
  } else if (IS_OBJ(value)) {
    printObject(value);
  }
}
