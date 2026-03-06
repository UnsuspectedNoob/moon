// chunk.h

#ifndef MOON_CHUNK_H
#define MOON_CHUNK_H

#include "common.h"
#include "value.h"

// The Instruction Set Architecture (ISA)
typedef enum {
  OP_CONSTANT,

  OP_NIL,   // New: for 'nil'
  OP_TRUE,  // New: for 'true'
  OP_FALSE, // New: for 'false'

  OP_POP, // New: to clean up stack after statements

  OP_GET_LOCAL,     // New: Access stack at index
  OP_SET_LOCAL,     // New: Write to stack at index
  OP_GET_GLOBAL,    // New: Get var
  OP_DEFINE_GLOBAL, // New: Let var
  OP_SET_GLOBAL,    // New: Set var

  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NEGATE,

  OP_PRINT, // New: for 'show'

  OP_JUMP_IF_FALSE, // Conditional Jump forward
  OP_JUMP,          // Unconditional Jump forward
  OP_LOOP,          // Unconditional Jump BACKWARD
  OP_EQUAL,         // New: == (is)
  OP_GREATER,       // New: >
  OP_LESS,          // New: <
  OP_NOT,           // New: not

  OP_BUILD_STRING,

  OP_BUILD_LIST,
  OP_GET_SUBSCRIPT,
  OP_SET_SUBSCRIPT,
  OP_GET_END_INDEX,

  OP_RANGE, // Creates a range object (1 to 10)

  OP_FOR_ITER,
  OP_GET_ITER, // <--- Add this

  OP_CALL,
  OP_CLOSURE,
  OP_RETURN,
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
