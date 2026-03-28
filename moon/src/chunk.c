// chunk.c

#include <stdlib.h>

#include "chunk.h"

void initChunk(Chunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  initValueArray(&chunk->constants);
}

void freeChunk(Chunk *chunk) {
  free(chunk->code);
  free(chunk->lines);
  freeValueArray(&chunk->constants);
  initChunk(chunk);
}

void writeChunk(Chunk *chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = (oldCapacity < 8) ? 8 : oldCapacity * 2;
    chunk->code =
        (uint8_t *)realloc(chunk->code, sizeof(uint8_t) * chunk->capacity);
    chunk->lines = (int *)realloc(chunk->lines, sizeof(int) * chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  chunk->lines[chunk->count] = line;
  chunk->count++;
}

extern bool valuesEqual(Value a, Value b);

int addConstant(Chunk *chunk, Value value) {
  // --- THE DEDUPLICATION LOOP ---
  // If we already have this exact number or string in the chunk, reuse it!
  for (int i = 0; i < chunk->constants.count; i++) {
    if (valuesEqual(chunk->constants.values[i], value)) {
      return i;
    }
  }

  // Otherwise, add it as a brand new constant
  writeValueArray(&chunk->constants, value);
  return chunk->constants.count - 1;
}
