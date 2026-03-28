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

uint32_t hashValue(Value value) {
  // 1. If it's an object (like a string), use the string's cached FNV-1a hash!
  if (IS_OBJ(value)) {
    if (IS_STRING(value))
      return AS_STRING(value)->hash;

    // For other objects (lists/dicts), we hash their memory pointer
    return (uint32_t)(uintptr_t)AS_OBJ(value);
  }

  // 2. If it's a number, boolean, or nil, use a fast integer hash
  uint64_t hash = value;
  hash = (~hash) + (hash << 18);
  hash = hash ^ (hash >> 31);
  hash = hash * 21;
  hash = hash ^ (hash >> 11);
  hash = hash + (hash << 6);
  hash = hash ^ (hash >> 22);

  return (uint32_t)hash;
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
