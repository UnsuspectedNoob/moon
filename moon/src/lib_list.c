#include <stdlib.h>
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

  // Pre-allocate exactly the right amount of memory to avoid O(log N) capacity resizing
  if (original->count > 0) {
    reversed->items = ALLOCATE(Value, original->count);
    reversed->capacity = original->count;
    reversed->count = original->count;

    for (int i = 0; i < original->count; i++) {
      reversed->items[i] = original->items[original->count - 1 - i];
    }
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

  StringBuffer sb;
  initBuffer(&sb);

  // Instead of building a full GC String object for each item,
  // we serialize them straight into our flat C buffer!
  for (int i = 0; i < list->count; i++) {
    stringifyValueToBuffer(list->items[i], 0, &sb);
    
    if (i < list->count - 1) {
      appendBuffer(&sb, delim->chars, delim->length);
    }
  }

  ObjString *result = takeString(sb.chars, sb.length);
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

static Value parseBaseNative(int argCount, Value *args) {
  if (argCount != 2)
    return NIL_VAL;
  if (!IS_NUMBER(args[1])) {
    throwNativeError("The base must be a number.", "Invalid radix type.");
  }

  double radix = AS_NUMBER(args[1]);
  Value seq = args[0];

  // 1. HORNER's METHOD FOR LISTS
  if (IS_LIST(seq)) {
    ObjList *list = AS_LIST(seq);
    double result = 0.0;

    for (int i = 0; i < list->count; i++) {
      if (!IS_NUMBER(list->items[i])) {
        throwNativeError("Lists parsed with a base can only contain numbers.",
                         "Found a %s in the list.", TYPE_NAME(list->items[i]));
      }
      result = (result * radix) + AS_NUMBER(list->items[i]);
    }
    return NUMBER_VAL(result);
  }
  // 2. STANDARD C-PARSING FOR STRINGS
  else if (IS_STRING(seq)) {
    ObjString *str = AS_STRING(seq);
    char *end;
    long result = strtol(str->chars, &end, (int)radix);

    if (*end != '\0') {
      throwNativeError("The string contains invalid characters for this base.",
                       "Cannot parse '%s' in base %d.", str->chars, (int)radix);
    }
    return NUMBER_VAL((double)result);
  }

  throwNativeError("You can only parse Strings or Lists.",
                   "Invalid sequence type.");
  return NIL_VAL;
}

// --- THE HANDSHAKE ---

void registerListLibrary() {
  REGISTER_PHRASE(NULL, "reverse", "$1", 1, "reverse$1", reverseNative, vm.listType);
  REGISTER_PHRASE(NULL, "join", "$1,with,$1", 2, "join$1_with$1", joinNative, vm.listType, vm.stringType);
  REGISTER_PHRASE(NULL, "pop", "from,$1", 1, "pop_from$1", popNative, vm.listType);
  REGISTER_PHRASE(NULL, "numbers", "in,$1,in,base,$1", 2, "numbers_in$1_in_base$1", parseBaseNative, vm.anyType, vm.numberType);
  REGISTER_PHRASE(NULL, "index", "of,$1,in,$1", 2, "index_of$1_in$1", indexOfNative, vm.anyType, vm.listType);
}
