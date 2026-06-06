#include <string.h>

#include "debug.h"
#include "emitter.h"
#include "parser.h" // We will create this next!

// ==========================================
// GLOBAL STATE
// ==========================================

Compiler *current = NULL;
ObjModule *currentModule = NULL;
Table *currentGlobals = NULL;
int currentLine = 0;

// ==========================================
// COMPILER INITIALIZATION
// ==========================================

void initCompiler(Compiler *compiler, FunctionType type) {
  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = type;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->temporaries = 0;

  compiler->function = newFunction();
  current = compiler;
  compiler->chunk = &compiler->function->chunk;

  // --- THE MODULE TAG ---
  compiler->function->module = currentModule;
  compiler->function->homeGlobals = currentGlobals;
  compiler->function->isTopLevel = (type == TYPE_SCRIPT);

  if (type != TYPE_SCRIPT) {
    current->function->name = copyString("<anonymous>", 11);
  }

  Local *local = &compiler->locals[compiler->localCount++];
  local->depth = 0;
  local->isCaptured = false;
  local->slot = 0;
  local->name.start = "";
  local->name.length = 0;
}

ObjFunction *endCompiler() {
  emitReturn(); // Emit OP_NIL, OP_RETURN for the end of the function

  ObjFunction *function = current->function;

#ifdef DEBUG_FUNCTION_NAME
  if ((vm.debugMode || printBytecodeFlag) && !parser.hadError) {
    disassembleChunk(currentChunk(), function->name != NULL
                                         ? function->name->chars
                                         : "<main>");
  }
#endif

  current = current->enclosing;
  return function;
}

// ==========================================
// SCOPE MANAGEMENT
// ==========================================

void beginScope() { current->scopeDepth++; }

void endScope() {
  current->scopeDepth--;

  int countToPop = 0;
  // Pop locals that fell out of scope
  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > current->scopeDepth) {
    countToPop++;
    current->localCount--;
  }

  while (countToPop > 255) {
    emitBytes(OP_POP_N, 255);
    countToPop -= 255;
  }
  if (countToPop == 1) {
    emitByte(OP_POP);
  } else if (countToPop > 1) {
    emitBytes(OP_POP_N, (uint8_t)countToPop);
  }
}

// ==========================================
// LOCAL VARIABLES
// ==========================================

Token syntheticToken(const char *text) {
  Token token;
  token.type = TOKEN_IDENTIFIER;
  token.start = text;
  token.length = (int)strlen(text);
  token.line = 0;
  return token;
}

bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

void addLocal(Token name) {
  if (current->localCount == UINT8_COUNT) {
    error("Too many local variables in function.");
    return;
  }

  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;

  local->slot = (current->localCount - 1) + current->temporaries;
}

int resolveLocal(Compiler *compiler, Token *name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer.");
      }
      return local->slot; // <--- RETURN THE PHYSICAL SLOT, NOT THE INDEX!
    }
  }
  return -1;
}

void declareVariable() {
  if (current->scopeDepth == 0)
    return;

  Token *name = &parser.previous;
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }
    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }

  addLocal(*name);
}

void markInitialized() {
  if (current->scopeDepth == 0)
    return;
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

void defineVariable(uint8_t global) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }
  emitBytes(OP_DEFINE_GLOBAL, global);
}

// ==========================================
// CHUNK MANAGEMENT & DRAFTING
// ==========================================

Chunk *currentChunk() { return current->chunk; }

// ==========================================
// BYTECODE EMISSION
// ==========================================

void emitByte(uint8_t byte) { writeChunk(currentChunk(), byte, currentLine); }

void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

void emitReturn() { emitBytes(OP_NIL, OP_RETURN); }

uint16_t makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);
  if (constant > 65535) { // <--- Upgrade from 255
    error("Too many constants in one chunk.");
    return 0;
  }
  return (uint16_t)constant;
}

void emitConstant(Value value) {
  if (IS_NUMBER(value)) {
    double num = AS_NUMBER(value);
    int intVal = (int)num;
    if (num == intVal && intVal >= 0 && intVal <= 255) {
      emitBytes(OP_PUSH_BYTE, (uint8_t)intVal);
      return;
    }
  }

  int constant = addConstant(currentChunk(), value);

  if (constant <= 255) {
    emitBytes(OP_CONSTANT, (uint8_t)constant);
  } else if (constant <= 65535) {
    emitByte(OP_CONSTANT_LONG);
    emitByte((constant >> 8) & 0xff);
    emitByte(constant & 0xff);
  } else {
    error("Too many constants in one chunk.");
  }
}

void emitGetLocal(int slot) {
  if (slot <= 255) {
    emitBytes(OP_GET_LOCAL, (uint8_t)slot);
  } else {
    emitByte(OP_GET_LOCAL_LONG);
    emitByte((slot >> 8) & 0xff);
    emitByte(slot & 0xff);
  }
}

void emitSetLocal(int slot) {
  if (slot <= 255) {
    emitBytes(OP_SET_LOCAL, (uint8_t)slot);
  } else {
    emitByte(OP_SET_LOCAL_LONG);
    emitByte((slot >> 8) & 0xff);
    emitByte(slot & 0xff);
  }
}

void emitForIter(int iterSlot) {
  if (iterSlot <= 255) {
    emitBytes(OP_FOR_ITER, (uint8_t)iterSlot);
  } else {
    emitByte(OP_FOR_ITER_LONG);
    emitByte((iterSlot >> 8) & 0xff);
    emitByte(iterSlot & 0xff);
  }
}

void emitGetIterValue(int iterSlot) {
  if (iterSlot <= 255) {
    emitBytes(OP_GET_ITER_VALUE, (uint8_t)iterSlot);
  } else {
    emitByte(OP_GET_ITER_VALUE_LONG);
    emitByte((iterSlot >> 8) & 0xff);
    emitByte(iterSlot & 0xff);
  }
}

// ==========================================
// CONTROL FLOW EMISSION
// ==========================================

int emitJump(uint8_t instruction) {
  emitByte(instruction);
  emitByte(0xff);
  emitByte(0xff);
  return currentChunk()->count - 2;
}

void patchJump(int offset) {
  int jump = currentChunk()->count - offset - 2;

  if (jump > UINT16_MAX) {
    error("Too much code to jump over.");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

void emitLoop(int loopStart) {
  emitByte(OP_LOOP);

  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX)
    error("Loop body too large.");

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}
