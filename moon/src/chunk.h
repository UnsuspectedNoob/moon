// chunk.h

#ifndef MOON_CHUNK_H
#define MOON_CHUNK_H

#include "value.h"

// The Instruction Set Architecture (ISA)
typedef enum {
  OP_CONSTANT,
  OP_CONSTANT_LONG,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_GET_GLOBAL,
  OP_DEFINE_GLOBAL,
  OP_SET_GLOBAL,
  OP_ADD,
  OP_ADD_INPLACE,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_MOD,
  OP_JUMP_IF_FALSE,
  OP_NEGATE,
  OP_JUMP,
  OP_LOOP,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_NOT,
  OP_BUILD_STRING,
  OP_BUILD_LIST,
  OP_BUILD_DICT,
  OP_BUILD_UNION,
  OP_GET_PROPERTY,
  OP_SET_PROPERTY,
  OP_GET_SUBSCRIPT,
  OP_SET_SUBSCRIPT,
  OP_GET_END_INDEX,
  OP_RANGE,
  OP_FOR_ITER,
  OP_GET_ITER,
  OP_GET_ITER_VALUE,
  OP_CALL,
  OP_TYPE_DEF,
  OP_INSTANTIATE,
  OP_DEFINE_METHOD,
  OP_CAST,
  OP_LOAD,
  OP_KEEP_LIST,
  OP_KEEP_DICT,
  OP_RETURN,
  OP_PUSH_SEQUENCE,
  OP_POP_SEQUENCE,
  OP_SHOW_REPL,
} OpCode;

typedef struct {
  int count;
  int capacity;
  uint8_t *code;        // The actual bytecode bytes
  int *lines;           // Line number for each byte (for debugging)
  ValueArray constants; // The pool of numbers/strings
} Chunk;

void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int line);
int addConstant(Chunk *chunk, Value value);

#endif
