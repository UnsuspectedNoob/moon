#ifndef MOON_DEBUG_H
#define MOON_DEBUG_H

#include "chunk.h"
#include "vm.h"

void disassembleChunk(Chunk *chunk, const char *name);
int disassembleInstruction(Chunk *chunk, int offset);
void debugStack(VM *vm);

#endif
