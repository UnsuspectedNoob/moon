#include <string.h>

#include "lib_list.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

// Extern so we can use your existing value comparators
extern bool valuesEqual(Value a, Value b);

// --- THE PRIMITIVES ---

static Value reverseNative(int argCount, Value *args) {
  if (argCount != 1)
    return NIL_VAL; // Arity Shield
  if (!IS_LIST(args[0]))
    return NIL_VAL; // Type Shield

  ObjList *original = AS_LIST(args[0]);
  ObjList *reversed = newList();

  push(OBJ_VAL(reversed)); // GC Protection!

  // Walk backward and append to the new list
  for (int i = original->count - 1; i >= 0; i--) {
    appendList(reversed, original->items[i]);
  }

  pop(); // Remove from VM stack
  return OBJ_VAL(reversed);
}

static Value joinNative(int argCount, Value *args) {
  if (argCount != 2)
    return NIL_VAL;
  if (!IS_LIST(args[0]) || !IS_STRING(args[1]))
    return NIL_VAL;

  ObjList *list = AS_LIST(args[0]);
  ObjString *delim = AS_STRING(args[1]);

  if (list->count == 0) {
    return OBJ_VAL(copyString("", 0));
  }

  // 1. Array to hold the stringified versions of each item
  ObjString **strings = ALLOCATE(ObjString *, list->count);
  int totalLength = 0;

  // 2. First Pass: Convert everything to a string and protect it from the GC!
  for (int i = 0; i < list->count; i++) {
    strings[i] = valueToString(list->items[i]);
    push(OBJ_VAL(strings[i])); // Shield from GC
    totalLength += strings[i]->length;
  }

  // Add room for the delimiters
  totalLength += delim->length * (list->count - 1);

  // 3. Second Pass: Allocate the exact memory needed and smash them together
  char *chars = ALLOCATE(char, totalLength + 1);
  char *dest = chars;

  for (int i = 0; i < list->count; i++) {
    memcpy(dest, strings[i]->chars, strings[i]->length);
    dest += strings[i]->length;

    if (i < list->count - 1) {
      memcpy(dest, delim->chars, delim->length);
      dest += delim->length;
    }
  }
  chars[totalLength] = '\0';

  // 4. Create the final MOON string
  ObjString *result = takeString(chars, totalLength);

  // 5. Cleanup: Pop the temporary strings off the VM stack and free the C array
  vm.stackTop -= list->count;
  FREE_ARRAY(ObjString *, strings, list->count);
  return OBJ_VAL(result);
}

static Value popNative(int argCount, Value *args) {
  if (argCount != 1)
    return NIL_VAL;
  if (!IS_LIST(args[0]))
    return NIL_VAL;

  ObjList *list = AS_LIST(args[0]);
  if (list->count == 0)
    return NIL_VAL;

  // Grab the last item, zero out the slot, and shrink the count
  Value lastItem = list->items[list->count - 1];
  list->items[list->count - 1] = NIL_VAL;
  list->count--;

  return lastItem;
}

static Value indexOfNative(int argCount, Value *args) {
  if (argCount != 2)
    return NIL_VAL;
  if (!IS_LIST(args[0]))
    return NIL_VAL;

  ObjList *list = AS_LIST(args[0]);
  Value searchItem = args[1];

  for (int i = 0; i < list->count; i++) {
    if (valuesEqual(list->items[i], searchItem)) {
      return NUMBER_VAL(i + 1); // MOON philosophy: 1-based indexing!
    }
  }

  return NIL_VAL; // Not found
}

// --- THE HANDSHAKE ---

void registerListLibrary() {
  defineNative("__list_reverse", reverseNative);
  defineNative("__list_join", joinNative);
  defineNative("__list_pop", popNative);
  defineNative("__list_index", indexOfNative);
}

// --- THE MOON WRAPPERS ---

const char listBootstrap[] = "let reverse (l: List):\n"
                             "    give __list_reverse(l)\n"
                             "end\n"
                             "\n"
                             "let join (l: List) with (delim: String):\n"
                             "    give __list_join(l, delim)\n"
                             "end\n"
                             "\n"
                             "let pop from (l: List):\n"
                             "    give __list_pop(l)\n"
                             "end\n"
                             "\n"
                             "let index of (item: Any) in (l: List):\n"
                             "    give __list_index(l, item)\n"
                             "end\n";
