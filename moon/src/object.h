// object.h
#ifndef MOON_OBJECT_H
#define MOON_OBJECT_H

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

typedef enum {
  OBJ_STRING,
  OBJ_RANGE,
  OBJ_LIST,
  OBJ_DICT,
  OBJ_FUNCTION,
  OBJ_NATIVE,
  OBJ_TYPE_BLUEPRINT,
  OBJ_INSTANCE,
  OBJ_MULTI_FUNCTION,
} ObjKind;

typedef struct Obj {
  ObjKind type;
  struct Obj *next; // Intrusion pointer for GC (we'll use this later)
} Obj;

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)

#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))

#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value))->function)

#define IS_LIST(value) isObjType(value, OBJ_LIST)
#define AS_LIST(value) ((ObjList *)AS_OBJ(value))

#define IS_DICT(value) isObjType(value, OBJ_DICT)
#define AS_DICT(value) ((ObjDict *)AS_OBJ(value))

#define IS_RANGE(value) isObjType(value, OBJ_RANGE)
#define AS_RANGE(value) ((ObjRange *)AS_OBJ(value))

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

// --- THE BLUEPRINT (e.g., "Player", "Stack") ---
typedef struct ObjType {
  Obj obj;
  ObjString *name;
  Table properties;
  bool isNative;
} ObjType;

// --- THE CLONE (The physical object in memory) ---
typedef struct {
  Obj obj;
  ObjType *type; // The invisible pointer back to its Blueprint!
  Table fields;  // The dictionary holding its actual data (health, name)
} ObjInstance;

// --- THE DISPATCHER (The container for overloaded methods) ---
typedef struct {
  Obj obj;
  ObjString *name; // The mangled name: "push$1_to$1"
  int arity;       // How many arguments this phrase takes
  int methodCount;
  int methodCapacity;
  ObjFunction **methods; // Array of actual compiled bytecode chunks
  ObjType ***signatures; // 2D Array! [Method Index][Argument Index] -> Points
                         // to ObjType
} ObjMultiFunction;

// Helper to check type
static inline bool isObjType(Value value, ObjKind type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

void printObject(Value value);

ObjString *copyString(const char *chars, int length);
ObjString *copyStringUnescaped(const char *chars, int length);
ObjString *takeString(char *chars, int length);

ObjFunction *newFunction();
ObjNative *newNative(NativeFn function);

ObjList *newList();
void appendList(ObjList *list, Value value);
void storeList(ObjList *list, int index, Value value);
Value indexList(ObjList *list, int index);
void deleteList(ObjList *list, int index);

uint32_t hashString(const char *key, int length);

ObjRange *newRange(double start, double end, double step);
ObjType *newType(ObjString *name);
ObjInstance *newInstance(ObjType *type);
ObjMultiFunction *newMultiFunction(ObjString *name, int arity);

ObjString *valueToString(Value value);

ObjDict *newDict();
#endif
