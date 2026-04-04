#ifndef MOON_VM_H
#define MOON_VM_H

#include "chunk.h"
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

  // --- GARBAGE COLLECTOR STATE ---
  size_t bytesAllocated;
  size_t nextGC;
  Obj **grayStack; // The array of objects we need to trace
  int grayCount;
  int grayCapacity;
  bool allowGC; // Protects the AST during compilation
} VM;

typedef struct {
  const char *name;
  NativeFn function;
} NativeDef;
ObjType *getObjType(Value val);

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char *source);

// --- THE NATIVE MODULE REGISTRY ---

typedef void (*NativeRegisterFn)();

typedef struct {
  const char *name;
  const char *moonWrapperSource;
  NativeRegisterFn registerCFunctions;
} MoonModule;

// --- THE NATIVE API ---
void push(Value value);
Value pop();
Value peek(int distance);
void defineNative(const char *name, NativeFn function);
void throwNativeError(const char *hint, const char *format, ...);

extern bool isCoreBootstrapped;

#endif
