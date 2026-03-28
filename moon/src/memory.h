// memory.h

#ifndef MOON_MEMORY_H
#define MOON_MEMORY_H

#include "common.h"
#include "object.h"

#define ALLOCATE(type, count)                                                  \
  (type *)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount)                          \
  (type *)reallocate(pointer, sizeof(type) * (oldCount),                       \
                     sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount)                                    \
  reallocate(pointer, sizeof(type) * (oldCount), 0)

void *reallocate(void *pointer, size_t oldSize, size_t newSize);
void freeObjects();

// --- DYNAMIC ARRAY GENERATORS ---

// 1. The Struct Blueprint Generator
#define DECLARE_ARRAY(Type, Name)                                              \
  typedef struct {                                                             \
    int capacity;                                                              \
    int count;                                                                 \
    Type *items;                                                               \
  } Name;                                                                      \
  void init##Name(Name *array);                                                \
  void write##Name(Name *array, Type item);                                    \
  void free##Name(Name *array);

// 2. The Logic Generator
#define DEFINE_ARRAY(Type, Name)                                               \
  void init##Name(Name *array) {                                               \
    array->items = NULL;                                                       \
    array->capacity = 0;                                                       \
    array->count = 0;                                                          \
  }                                                                            \
  void write##Name(Name *array, Type item) {                                   \
    if (array->capacity < array->count + 1) {                                  \
      int oldCapacity = array->capacity;                                       \
      array->capacity = GROW_CAPACITY(oldCapacity);                            \
      array->items =                                                           \
          GROW_ARRAY(Type, array->items, oldCapacity, array->capacity);        \
    }                                                                          \
    array->items[array->count] = item;                                         \
    array->count++;                                                            \
  }                                                                            \
  void free##Name(Name *array) {                                               \
    FREE_ARRAY(Type, array->items, array->capacity);                           \
    init##Name(array);                                                         \
  }

#endif
