#ifndef MOON_VM_H
#define MOON_VM_H

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 256
#define STACK_MAX (FRAMES_MAX * 256)

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

  Table globals;
  Table strings;

  Obj *objects; // for the GC

  bool debugMode; // <--- ADD THIS FLAG

  ObjString *charStrings[256];
} VM;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char *source);
void push(Value value);
Value pop();

#endif
