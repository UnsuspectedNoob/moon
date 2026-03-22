#ifndef MOON_DEBUG_H
#define MOON_DEBUG_H

#include "ast.h"
#include "chunk.h"
#include "vm.h"

void disassembleChunk(Chunk *chunk, const char *name);
int disassembleInstruction(Chunk *chunk, int offset);
void debugStack(VM *vm);

extern bool printAstFlag;
// Add this near your disassembleChunk prototype
void printAST(Node *node, int indent);

#endif
