// object.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType)                                         \
  (type *)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(size_t size, ObjType type) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);
  object->type = type;

  // Link to the VM's list of objects (for future GC)
  object->next = vm.objects;
  vm.objects = object;
  return object;
}

// FNV-1a Hash Algorithm
uint32_t hashString(const char *key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

static ObjString *allocateString(char *chars, int length, uint32_t hash) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  //  FIX: Add the new string to the interned set immediately
  tableSet(&vm.strings, string, NIL_VAL);

  return string;
}

// ... hashString function ...

ObjString *takeString(char *chars, int length) {
  uint32_t hash = hashString(chars, length);

  // Check if we already have it
  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars,
               length + 1); // We don't need the passed-in chars anymore
    return interned;
  }

  return allocateString(chars, length, hash);
}

ObjString *copyString(const char *chars, int length) {
  uint32_t hash = hashString(chars, length);

  // Check if we already have it
  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL)
    return interned;

  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash);
}

ObjString *copyStringUnescaped(const char *chars, int length) {
  // Optimistic allocation: The result will be at most 'length' bytes.
  char *heapChars = ALLOCATE(char, length + 1);
  int dest = 0;

  for (int i = 0; i < length; i++) {
    // Check for double backtick escape
    if (chars[i] == '`' && i + 1 < length && chars[i + 1] == '`') {
      heapChars[dest++] = '`'; // Write one
      i++;                     // Skip the second one
    } else {
      heapChars[dest++] = chars[i];
    }
  }

  heapChars[dest] = '\0';

  // takeString hashes the content, checks the intern table,
  // and either adopts our 'heapChars' or frees it and returns the existing one.
  return takeString(heapChars, dest);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_DICT:
    printf("<dict>");
    break;

  case OBJ_RANGE: {
    ObjRange *range = AS_RANGE(value);
    // We print using %.14g to strip trailing zeros (1.0 -> 1)
    printf("%.14g to %.14g", range->start, range->end);
    break;
  }

  case OBJ_LIST: {
    ObjList *list = AS_LIST(value);
    printf("[");
    for (int i = 0; i < list->count; i++) {
      printValue(list->items[i]);
      if (i < list->count - 1)
        printf(", ");
    }

    printf("]");
    break;
  }

  case OBJ_STRING:
    printf("\"%s\"", AS_CSTRING(value));
    break;

  case OBJ_FUNCTION: {
    if (AS_FUNCTION(value)->name == NULL) {
      printf("<script>");
    } else {
      printf("<fn %s>", AS_FUNCTION(value)->name->chars);
    }
    break;
  }

  case OBJ_NATIVE:
    printf("<native fn>");
    break;
  }
}

ObjFunction *newFunction() {
  ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);

  function->arity = 0;
  function->name = NULL;
  initChunk(&function->chunk);

  return function;
}

ObjNative *newNative(NativeFn function) {
  ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
  return native;
}

ObjList *newList() {
  ObjList *list = ALLOCATE_OBJ(ObjList, OBJ_LIST);

  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
  return list;
}

ObjDict *newDict() {
  ObjDict *dict = ALLOCATE_OBJ(ObjDict, OBJ_DICT);
  initTable(&dict->fields);
  return dict;
}

void appendList(ObjList *list, Value value) {
  if (list->capacity < list->count + 1) {
    int oldCapacity = list->capacity;
    list->capacity = GROW_CAPACITY(oldCapacity);
    list->items = GROW_ARRAY(Value, list->items, oldCapacity, list->capacity);
  }

  list->items[list->count] = value;
  list->count++;
}

void storeList(ObjList *list, int index, Value value) {
  // Note: Bounds checking should happen in the VM before calling this
  list->items[index] = value;
}

Value indexList(ObjList *list, int index) { return list->items[index]; }

void deleteList(ObjList *list, int index) {
  for (int i = index; i < list->count - 1; i++) {
    list->items[i] = list->items[i + 1];
  }
  list->items[list->count - 1] = NIL_VAL; // Clear the last slot
  list->count--;
}

ObjRange *newRange(double start, double end, double step) {
  ObjRange *range = ALLOCATE_OBJ(ObjRange, OBJ_RANGE);

  range->start = start;
  range->end = end;
  range->step = step;
  return range;
}
