// object.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

DECLARE_ARRAY(ObjString *, StringArray)
DEFINE_ARRAY(ObjString *, StringArray);

#define ALLOCATE_OBJ(type, objectType)                                         \
  (type *)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(size_t size, ObjKind type) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);
  object->type = type;
  object->isMarked = false;

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
  string->left = NULL;  // <--- Initialize to NULL
  string->right = NULL; // <--- Initialize to NULL

  // Push to VM stack to synchronize with GC
  push(OBJ_VAL(string));
  tableSet(&vm.strings, OBJ_VAL(string), NIL_VAL);
  pop();

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

ObjString *takeRope(ObjString *left, ObjString *right) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);

  string->length = left->length + right->length;
  string->chars = NULL; // NULL means "I am a Rope!"
  string->hash = 0;     // Unhashed until flattened
  string->left = left;
  string->right = right;

  // Notice we do NOT intern ropes into vm.strings!
  // They are completely ephemeral and invisible to the Hash Table.
  return string;
}

void flattenString(ObjString *string) {
  if (string->chars != NULL)
    return; // Fast path: It is already flat!

  // Allocate the exact size needed for the entire combined string
  char *buffer = ALLOCATE(char, string->length + 1);
  int offset = 0;

  // --- THE CALL STACK BOMB DEFUSER (Iterative DFS) ---
  StringArray worklist;
  initStringArray(&worklist);
  writeStringArray(&worklist, string);

  while (worklist.count > 0) {
    // Pop the top node off our worklist
    ObjString *currentNode = worklist.items[--worklist.count];

    if (currentNode->chars != NULL) {
      // Base Case: It's a flat leaf node!
      memcpy(buffer + offset, currentNode->chars, currentNode->length);
      offset += currentNode->length;
    } else {
      // Recursive Case: It's a Rope!
      // Because this is a LIFO stack (Last In, First Out), we push the RIGHT
      // branch first, so the LEFT branch ends up on top and gets processed
      // first!
      if (currentNode->right != NULL)
        writeStringArray(&worklist, currentNode->right);
      if (currentNode->left != NULL)
        writeStringArray(&worklist, currentNode->left);
    }
  }

  freeStringArray(&worklist);
  // ---------------------------------------------------

  buffer[string->length] = '\0';

  // Mutate the Rope into a standard flat string
  string->chars = buffer;
  string->hash = hashString(buffer, string->length);

  // Sever the branches!
  string->left = NULL;
  string->right = NULL;
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
    printf("<%.14g to %.14g>", range->start, range->end);
    break;
  }

  case OBJ_LIST: {
    printf("%s", "<list>");
    // ObjList *list = AS_LIST(value);
    // printf("[");
    // for (int i = 0; i < list->count; i++) {
    //   printValue(list->items[i]);
    //   if (i < list->count - 1)
    //     printf(", ");
    // }
    //
    // printf("]");
    break;
  }

  case OBJ_STRING:
    printf("%s", AS_CSTRING(value));
    break;

  case OBJ_FUNCTION: {
    if (AS_FUNCTION(value)->name == NULL) {
      printf("<script>");
    } else {
      printf("<fn %s>", AS_FUNCTION(value)->name->chars);
    }
    break;
  }

  case OBJ_TYPE_BLUEPRINT:
    printf("<type %s>", AS_TYPE(value)->name->chars);
    break;

  case OBJ_INSTANCE:
    printf("<%s instance>", AS_INSTANCE(value)->type->name->chars);
    break;

  case OBJ_MULTI_FUNCTION:
    printf("<multi-fn %s>", AS_MULTI_FUNCTION(value)->name->chars);
    break;

  case OBJ_NATIVE:
    printf("<native fn>");
    break;

  case OBJ_UNION:
    printf("<union type>");
    break;

  default:
    printf("<unknown>");
    break;
  }
}

ObjFunction *newFunction() {
  ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
  function->name = NULL;
  function->moduleName = NULL;
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

ObjType *newType(ObjString *name) {
  ObjType *type = ALLOCATE_OBJ(ObjType, OBJ_TYPE_BLUEPRINT);
  type->name = name;
  initTable(&type->properties);
  type->isNative = false;
  return type;
}

ObjInstance *newInstance(ObjType *type) {
  ObjInstance *instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
  instance->type = type;
  initTable(&instance->fields); // Boot up the empty hash table for data!
  return instance;
}

ObjMultiFunction *newMultiFunction(ObjString *name, int arity) {
  ObjMultiFunction *multi = ALLOCATE_OBJ(ObjMultiFunction, OBJ_MULTI_FUNCTION);
  multi->name = name;
  multi->arity = arity;
  multi->methodCount = 0;
  multi->methodCapacity = 0;
  multi->methods = NULL;
  multi->signatures = NULL;
  return multi;
}

ObjUnion *newUnion(int count) {
  ObjUnion *unionObj = ALLOCATE_OBJ(ObjUnion, OBJ_UNION);

  // 1. Lock into a GC-Safe State BEFORE allocating the array!
  unionObj->count = 0;
  unionObj->types = NULL;

  push(OBJ_VAL(unionObj)); // Shield it!

  // 2. Trigger allocation. If this fires the GC, blackenObject will
  // safely skip tracing because count is 0!
  unionObj->types = ALLOCATE(Value, count);

  // 3. Initialize the raw C-memory with safe values. If the GC runs
  // later, it won't crash trying to trace random C garbage.
  for (int i = 0; i < count; i++) {
    unionObj->types[i] = NIL_VAL;
  }

  // 4. Fully baked! Expose the count to the GC.
  unionObj->count = count;

  pop(); // Unshield it!
  return unionObj;
}

// Pre-declare our recursive engine
static ObjString *stringifyValue(Value value, int indent);

// The public interface kicks off the recursion at depth 0!
ObjString *valueToString(Value value) { return stringifyValue(value, 0); }

// The internal engine that tracks indentation
static ObjString *stringifyValue(Value value, int indent) {
  if (IS_STRING(value)) {
    flattenString(AS_STRING(value)); // <--- SHIELD
    return AS_STRING(value);
  }

  if (IS_NUMBER(value)) {
    double number = AS_NUMBER(value);
    char buffer[32];
    int length = sprintf(buffer, "%.14g", number);
    return copyString(buffer, length);
  }

  if (IS_BOOL(value)) {
    return AS_BOOL(value) ? copyString("true", 4) : copyString("false", 5);
  }

  if (IS_NIL(value)) {
    return copyString("nil", 3);
  }

  // --- THE DICTIONARY PRETTY PRINTER ---
  if (IS_DICT(value)) {
    ObjDict *dict = AS_DICT(value);

    // Empty dict fallback
    if (dict->fields.count == 0)
      return copyString("{}", 2);

    int capacity = 128;
    int length = 0;
    char *buffer = ALLOCATE(char, capacity);

    buffer[length++] = '{';
    buffer[length++] = '\n';

    bool firstItem = true;

    for (int i = 0; i < dict->fields.capacity; i++) {
      Entry *entry = &dict->fields.entries[i];

      if (IS_EMPTY(entry->key) || IS_TOMB(entry->key))
        continue;

      // 1. Recursively stringify, increasing the indent!
      ObjString *keyStr = stringifyValue(entry->key, indent + 1);
      push(OBJ_VAL(keyStr));

      ObjString *valStr = stringifyValue(entry->value, indent + 1);
      push(OBJ_VAL(valStr));

      // 2. Pre-calculate the maximum space this item will need
      int spaceNeeded = 2 + ((indent + 1) * 2) + keyStr->length + 2 +
                        valStr->length + (indent * 2) + 4;

      while (length + spaceNeeded > capacity) {
        int oldCapacity = capacity;
        capacity *= 2;
        buffer = (char *)reallocate(buffer, sizeof(char) * oldCapacity,
                                    sizeof(char) * capacity);
      }

      if (!firstItem) {
        buffer[length++] = ',';
        buffer[length++] = '\n';
      }
      firstItem = false;

      // 3. Add the Indentation
      for (int s = 0; s < (indent + 1) * 2; s++)
        buffer[length++] = ' ';

      // 4. Add the Key
      memcpy(buffer + length, keyStr->chars, keyStr->length);
      length += keyStr->length;

      // 5. Add the Colon
      buffer[length++] = ':';
      buffer[length++] = ' ';

      // 6. Add the Value
      memcpy(buffer + length, valStr->chars, valStr->length);
      length += valStr->length;

      pop();
      pop();
    }

    buffer[length++] = '\n';

    // Add the closing brace indentation
    for (int s = 0; s < indent * 2; s++)
      buffer[length++] = ' ';

    buffer[length++] = '}';
    buffer[length] = '\0';

    ObjString *result = copyString(buffer, length);
    FREE_ARRAY(char, buffer, capacity);
    return result;
  }

  // --- THE HORIZONTAL LIST PRINTER ---
  if (IS_LIST(value)) {
    ObjList *list = AS_LIST(value);
    if (list->count == 0)
      return copyString("[]", 2);

    int capacity = 128;
    int length = 0;
    char *buffer = ALLOCATE(char, capacity);

    buffer[length++] = '[';

    for (int i = 0; i < list->count; i++) {
      // Recursively stringify (we pass the same indent so if a Dict is
      // inside this list, it formats cleanly relative to the list)
      ObjString *itemStr = stringifyValue(list->items[i], indent);
      push(OBJ_VAL(itemStr));

      // We only need enough space for the item, comma, space, and brackets
      int spaceNeeded = itemStr->length + 4;
      while (length + spaceNeeded > capacity) {
        int oldCapacity = capacity;
        capacity *= 2;
        buffer = (char *)reallocate(buffer, sizeof(char) * oldCapacity,
                                    sizeof(char) * capacity);
      }

      memcpy(buffer + length, itemStr->chars, itemStr->length);
      length += itemStr->length;

      if (i < list->count - 1) {
        buffer[length++] = ',';
        buffer[length++] = ' ';
      }

      pop();
    }

    buffer[length++] = ']';
    buffer[length] = '\0';

    ObjString *result = copyString(buffer, length);
    FREE_ARRAY(char, buffer, capacity);
    return result;
  }

  return copyString("<object>", 8);
}
