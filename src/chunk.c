// chunk.c

#include <stdlib.h>

#include "chunk.h"

void initChunk(Chunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  initValueArray(&chunk->constants);
  
  chunk->caches.capacity = 0;
  chunk->caches.count = 0;
  chunk->caches.entries = NULL;
}

void freeChunk(Chunk *chunk) {
  free(chunk->code);
  free(chunk->lines);
  freeValueArray(&chunk->constants);
  free(chunk->caches.entries);
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

int addCacheEntry(Chunk *chunk) {
  if (chunk->caches.capacity < chunk->caches.count + 1) {
    int oldCapacity = chunk->caches.capacity;
    chunk->caches.capacity = (oldCapacity < 8) ? 8 : oldCapacity * 2;
    chunk->caches.entries = (InlineCacheEntry *)realloc(
        chunk->caches.entries, sizeof(InlineCacheEntry) * chunk->caches.capacity);
  }
  
  InlineCacheEntry *entry = &chunk->caches.entries[chunk->caches.count];
  entry->type = CACHE_EMPTY;
  entry->receiverType = NULL;
  for(int i = 0; i < MAX_CACHE_ARGS; i++) {
      entry->argTypes[i] = NULL;
  }
  entry->cachedValue = NIL_VAL;
  
  chunk->caches.count++;
  return chunk->caches.count - 1;
}
