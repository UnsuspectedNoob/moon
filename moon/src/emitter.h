#ifndef MOON_EMITTER_H
#define MOON_EMITTER_H

#include "chunk.h"
#include "common.h"
#include "object.h"
#include "scanner.h"

// ==========================================
// COMPILER STATE & SCOPE
// ==========================================

typedef enum { TYPE_FUNCTION, TYPE_SCRIPT } FunctionType;

typedef struct {
  Token name;
  int depth;
  bool isCaptured;
  int slot; // physical VM slot
} Local;

typedef struct Compiler {
  struct Compiler *enclosing; // Parent compiler (e.g., script -> function)
  ObjFunction *function;      // The function being compiled
  FunctionType type;          // Is this the main script or a user function?
  Chunk *chunk;
  Local locals[UINT8_COUNT];
  int localCount;
  int scopeDepth;
  int temporaries;
} Compiler;

extern Compiler *current;
extern ObjString *currentModuleName;
extern int currentLine;

// ==========================================
// PUBLIC API
// ==========================================

// Compiler State
void initCompiler(Compiler *compiler, FunctionType type);
ObjFunction *endCompiler();

// Scopes
void beginScope();
void endScope();

// Local Variables
Token syntheticToken(const char *text);
bool identifiersEqual(Token *a, Token *b);
void addLocal(Token name);
int resolveLocal(Compiler *compiler, Token *name);
void declareVariable();
void markInitialized();
void defineVariable(uint8_t global);

// Chunk Management
Chunk *currentChunk();

// Bytecode Emission
void emitByte(uint8_t byte);
void emitBytes(uint8_t byte1, uint8_t byte2);
void emitReturn();
uint16_t makeConstant(Value value);
void emitConstant(Value value);

// Control Flow Emission
int emitJump(uint8_t instruction);
void patchJump(int offset);
void emitLoop(int loopStart);

#endif
