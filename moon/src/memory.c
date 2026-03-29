// memory.c

#include <stdlib.h>

#include "memory.h"
#include "object.h"
#include "vm.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  (void)oldSize; // Silence unused parameter warning

  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void *result = realloc(pointer, newSize);
  if (result == NULL)
    exit(1);
  return result;
}

void freeObject(Obj *object) {
  switch (object->type) {
  case OBJ_RANGE: {
    FREE(ObjRange, object);
    break;
  }

  case OBJ_LIST: {
    ObjList *list = (ObjList *)object;
    FREE_ARRAY(Value, list->items, list->capacity);
    FREE(ObjList, object);
    break;
  }

  case OBJ_STRING: {
    ObjString *string = (ObjString *)object;
    FREE_ARRAY(char, string->chars, string->length + 1);
    FREE(ObjString, object);
    break;
  }

  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    freeChunk(&function->chunk); // Free the bytecode!
    FREE(ObjFunction, object);
    break;
  }

  case OBJ_DICT: {
    ObjDict *dict = (ObjDict *)object;
    freeTable(&dict->fields); // Free the hash table array
    FREE(ObjDict, object);    // Free the object wrapper itself
    break;
  }

  case OBJ_NATIVE: {
    FREE(ObjNative, object);
    break;
  }

  case OBJ_TYPE_BLUEPRINT: {
    ObjType *type = (ObjType *)object;
    freeTable(&type->properties);
    FREE(ObjType, object);
    break;
  }

  case OBJ_INSTANCE: {
    ObjInstance *instance = (ObjInstance *)object;
    freeTable(&instance->fields); // Free the hash table!
    FREE(ObjInstance, object);
    break;
  }

  case OBJ_MULTI_FUNCTION: {
    ObjMultiFunction *multi = (ObjMultiFunction *)object;

    // Free the 2D signatures array
    if (multi->signatures != NULL) {
      for (int i = 0; i < multi->methodCount; i++) {
        if (multi->signatures[i] != NULL) {
          FREE_ARRAY(ObjType *, multi->signatures[i], multi->arity);
        }
      }
      FREE_ARRAY(ObjType **, multi->signatures, multi->methodCapacity);
    }

    // Free the methods array
    if (multi->methods != NULL) {
      FREE_ARRAY(ObjFunction *, multi->methods, multi->methodCapacity);
    }

    FREE(ObjMultiFunction, object);
    break;
  }
  }
}

void freeObjects() {
  Obj *object = vm.objects;
  while (object != NULL) {
    Obj *next = object->next;
    freeObject(object);
    object = next;
  }
}
