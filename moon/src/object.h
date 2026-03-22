// object.h
#ifndef MOON_OBJECT_H
#define MOON_OBJECT_H

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)

#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)

#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value))->function)

#define IS_LIST(value) isObjType(value, OBJ_LIST)
#define AS_LIST(value) ((ObjList *)AS_OBJ(value))

#define IS_DICT(value) isObjType(value, OBJ_DICT)
#define AS_DICT(value) ((ObjDict *)AS_OBJ(value))

#define IS_RANGE(value) isObjType(value, OBJ_RANGE)
#define AS_RANGE(value) ((ObjRange *)AS_OBJ(value))

typedef enum {
  OBJ_STRING,
  OBJ_FUNCTION,
  OBJ_NATIVE,
  OBJ_LIST,
  OBJ_RANGE,
  OBJ_DICT,
} ObjType;

typedef struct Obj {
  ObjType type;
  struct Obj *next; // Intrusion pointer for GC (we'll use this later)
} Obj;

// The String Object
typedef struct ObjString {
  Obj obj;
  int length;
  char *chars;
  uint32_t hash; // Cached hash for fast map lookups
} ObjString;

typedef struct ObjFunction ObjFunction; // Forward declaration

typedef Value (*NativeFn)(int argCount, Value *args); // C Function pointer

// The "Blueprint" for a user function
struct ObjFunction {
  Obj obj;
  int arity;       // Number of arguments it expects
  Chunk chunk;     // The bytecode
  ObjString *name; // Function name (for debugging)
};

// The wrapper for a C function
typedef struct {
  Obj obj;
  NativeFn function;
} ObjNative;

typedef struct {
  Obj obj;      // The base class (header)
  int count;    // How many items are currently in the list
  int capacity; // How much memory we have allocated
  Value *items; // The pointer to the heap array
} ObjList;

typedef struct {
  Obj obj;
  Table fields; // We literally just reuse your existing Hash Table!
} ObjDict;

typedef struct {
  Obj obj;      // Base class
  double start; // Starting number
  double end;   // Ending number
  double step;
} ObjRange;

// Helper to check type
static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

ObjString *copyString(const char *chars, int length);
ObjString *copyStringUnescaped(const char *chars, int length);
ObjString *takeString(char *chars, int length);
void printObject(Value value);
ObjFunction *newFunction();
ObjNative *newNative(NativeFn function);

ObjList *newList();
void appendList(ObjList *list, Value value);
void storeList(ObjList *list, int index, Value value);
Value indexList(ObjList *list, int index);
void deleteList(ObjList *list, int index);

uint32_t hashString(const char *key, int length);

ObjRange *newRange(double start, double end, double step);

ObjDict *newDict();
#endif
