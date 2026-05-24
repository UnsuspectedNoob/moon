#ifndef MOON_VM_H
#define MOON_VM_H

#include "chunk.h"
#include "error.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 2048
#define STACK_MAX 256 * 256

// Represents one running function call
typedef struct {
  ObjFunction *function; // Which function is running?
  uint8_t *ip;           // Where are we in that function?
  Value *slots;          // Where do this function's locals start on the stack?
  Table *globals;        // The module's global scope!
  Value stickySubject;   // The active sticky subject for chained comparisons!
} CallFrame;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frameCount;

  Value stack[STACK_MAX];
  Value *stackTop;

  Table globals, strings, loadedModules;

  Obj *objects; // for the GC

  bool debugMode; // <--- ADD THIS FLAG

  ObjString *charStrings[256];

  // --- THE NATIVE TYPE REGISTRY ---
  ObjType *anyType;
  ObjType *typeType;
  ObjType *numberType;
  ObjType *stringType;
  ObjType *listType;
  ObjType *dictType;
  ObjType *boolType;
  ObjType *rangeType;
  ObjType *functionType;
  ObjType *nilType;
  ObjType *moduleType;

  // --- GARBAGE COLLECTOR STATE ---
  size_t bytesAllocated;
  size_t nextGC;
  Obj **grayStack; // The array of objects we need to trace
  int grayCount;
  int grayCapacity;
  bool allowGC; // Protects the AST during compilation

  Value sequenceStack[256];
  int sequenceCount;

  Value stickyStack[256];
  int stickyCount;
} VM;

ObjType *getObjType(Value val);
#define TYPE_NAME(val) (getObjType(val)->name->chars)

extern VM vm;
extern bool isReplMode;

void initVM();
void freeVM();
InterpretResult interpret(const char *source, int startLine);

// --- THE STATIC NATIVE REGISTRY API ---

// The Static Linker
void registerNativePhrasal(ObjModule *module, const char *root, const char *path,
                           int arity, const char *mangledName, NativeFn function,
                           ObjType **expectedTypes);

// Macros to make writing standard libraries incredibly clean
#define REGISTER_PHRASE(module, root, path, arity, mangledName, fn, ...)       \
  do {                                                                         \
    ObjType *types[] = {__VA_ARGS__};                                          \
    registerNativePhrasal(module, root, path, arity, mangledName, fn, types);  \
  } while (0)

#define REGISTER_PHRASE_0(module, root, path, mangledName, fn)                 \
  registerNativePhrasal(module, root, path, 0, mangledName, fn, NULL)

// --- THE NATIVE API ---
void push(Value value);
Value pop();
Value peek(int distance);
bool isFalsey(Value value);

// The Static Linker (We will implement this next)
void throwNativeError(const char *hint, const char *format, ...);
void runtimeErrorDetailed(ErrorType type, const char *hint, const char *format,
                          ...);

extern bool isCoreBootstrapped;

#endif
