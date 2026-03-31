// memory.c

#include <stdlib.h>

#include "memory.h"
#include "object.h"
#include "vm.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  // 1. Update the VM's memory tracker!
  // (If newSize is 0, this subtracts the oldSize perfectly!)
  vm.bytesAllocated += newSize - oldSize;

  // 2. The Auto-Trigger!
  if (newSize > oldSize) {
    // Only trigger if the AST is done parsing AND we crossed the memory limit
    if (vm.allowGC && vm.bytesAllocated > vm.nextGC) {
      collectGarbage();
    }
  }

  // 3. The Actual C-Allocation
  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void *result = realloc(pointer, newSize);
  if (result == NULL)
    exit(1);
  return result;
}

void markObject(Obj *object) {
  if (object == NULL)
    return;
  if (object->isMarked)
    return; // Already saved!

#ifdef DEBUG_PRINT_CODE
  if (vm.debugMode) {
    printf("  [GC] Marked ");
    printValue(OBJ_VAL(object));
    printf("\n");
  }
#endif

  object->isMarked = true;

  // Add it to the Gray Stack (the GC Worklist)
  if (vm.grayCapacity < vm.grayCount + 1) {
    vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
    // We use raw C realloc here so we don't accidentally trigger a nested GC
    // cycle!
    vm.grayStack =
        (Obj **)realloc(vm.grayStack, sizeof(Obj *) * vm.grayCapacity);
    if (vm.grayStack == NULL)
      exit(1);
  }

  vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
  if (IS_OBJ(value))
    markObject(AS_OBJ(value));
}

// --- GC HELPER: Mark Tables ---
static void markTable(Table *table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];
    markValue(entry->key);
    markValue(entry->value);
  }
}

// --- GC HELPER: Mark Constant Arrays ---
static void markArray(ValueArray *array) {
  for (int i = 0; i < array->count; i++) {
    markValue(array->values[i]);
  }
}

// --- PHASE 1: FIND THE ROOTS ---
static void markRoots() {
  // 1. The Stack
  for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
    markValue(*slot);
  }

  // 2. The CallFrames (The active functions/closures)
  for (int i = 0; i < vm.frameCount; i++) {
    markObject((Obj *)vm.frames[i].function);
  }

  // 3. The Globals
  markTable(&vm.globals);

  // 4. The Native Blueprints
  markObject((Obj *)vm.anyType);
  markObject((Obj *)vm.numberType);
  markObject((Obj *)vm.stringType);
  markObject((Obj *)vm.listType);
  markObject((Obj *)vm.dictType);
  markObject((Obj *)vm.boolType);
  markObject((Obj *)vm.rangeType);
  markObject((Obj *)vm.functionType);
  markObject((Obj *)vm.nilType);

  // 5. The Cached 1-Character Strings
  for (int i = 0; i < 256; i++) {
    if (vm.charStrings[i] != NULL) {
      markObject((Obj *)vm.charStrings[i]);
    }
  }
}

// --- PHASE 2: TRACE THE GRAPH ---
static void blackenObject(Obj *object) {
#ifdef DEBUG_PRINT_CODE
  if (vm.debugMode) {
    printf("  [GC] Blacken ");
    printValue(OBJ_VAL(object));
    printf("\n");
  }
#endif

  switch (object->type) {
  case OBJ_LIST: {
    ObjList *list = (ObjList *)object;
    for (int i = 0; i < list->count; i++) {
      markValue(list->items[i]); // Rescue every item in the list!
    }
    break;
  }

  case OBJ_DICT: {
    ObjDict *dict = (ObjDict *)object;
    markTable(&dict->fields); // Rescue every key and value in the dictionary!
    break;
  }

  case OBJ_INSTANCE: {
    ObjInstance *instance = (ObjInstance *)object;
    markObject((Obj *)instance->type); // Rescue its Blueprint!
    markTable(&instance->fields);      // Rescue its custom properties!
    break;
  }

  case OBJ_TYPE_BLUEPRINT: {
    ObjType *type = (ObjType *)object;
    markObject((Obj *)type->name); // Rescue the Blueprint's name
    markTable(&type->properties);  // Rescue its default methods/properties!
    break;
  }

  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    markObject((Obj *)function->name);
    markArray(&function->chunk
                   .constants); // Rescue the literals trapped in the bytecode!
    break;
  }

  case OBJ_MULTI_FUNCTION: {
    ObjMultiFunction *multi = (ObjMultiFunction *)object;
    markObject((Obj *)multi->name);

    for (int i = 0; i < multi->methodCount; i++) {
      markObject((Obj *)multi->methods[i]); // Rescue the compiled chunks!

      // Rescue the 2D array of Type Signatures!
      if (multi->signatures[i] != NULL) {
        for (int j = 0; j < multi->arity; j++) {
          if (multi->signatures[i][j] != NULL) {
            markObject((Obj *)multi->signatures[i][j]);
          }
        }
      }
    }
    break;
  }

  case OBJ_STRING:
  case OBJ_NATIVE:
  case OBJ_RANGE:
    // Leaf Nodes: These objects do not contain other objects.
    // There is nothing to trace, so we just break!
    break;
  }
}

static void traceReferences() {
  // As long as there are gray objects left to process...
  while (vm.grayCount > 0) {
    // Pop it, and Blacken it!
    // (Note: Blackening might discover new objects and push them to the Gray
    // Stack, which is why this is a while loop, not a for loop!)
    Obj *object = vm.grayStack[--vm.grayCount];
    blackenObject(object);
  }
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

// --- PHASE 4: THE EXECUTIONER ---
static void sweep() {
  Obj *previous = NULL;
  Obj *object = vm.objects;

  while (object != NULL) {
    if (object->isMarked) {
      // 1. SURVIVOR: Unmark it so it's ready for the next GC cycle
      object->isMarked = false;
      previous = object;
      object = object->next;
    } else {
      // 2. GARBAGE: Unlink it and free it!
      Obj *unreached = object;
      object = object->next;

      // Snip it out of the linked list
      if (previous != NULL) {
        previous->next = object;
      } else {
        vm.objects = object; // It was the head of the list
      }

      freeObject(unreached); // Incinerate!
    }
  }
}

// --- THE MASTER GC ROUTINE ---
void collectGarbage() {
#ifdef DEBUG_PRINT_CODE
  if (vm.debugMode)
    printf("-- GC BEGIN\n");
  size_t before = vm.bytesAllocated;
#endif

  markRoots();                   // Phase 1: Save the immediate variables
  traceReferences();             // Phase 2: Traverse their children
  tableRemoveWhite(&vm.strings); // Phase 3: Purge dead weak strings
  sweep();                       // Phase 4: Burn the rest

  // Phase 5: Calculate the next GC threshold!
  // We dynamically scale the threshold based on how much memory survived.
  vm.nextGC = vm.bytesAllocated * 2;

#ifdef DEBUG_PRINT_CODE
  if (vm.debugMode) {
    printf("-- GC END\n");
    printf("   Collected %zu bytes (from %zu to %zu) next at %zu\n",
           before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
  }
#endif
}

void freeObjects() {
  Obj *object = vm.objects;
  while (object != NULL) {
    Obj *next = object->next;
    freeObject(object);
    object = next;
  }
}
