// scout.h
#ifndef MOON_SCOUT_H
#define MOON_SCOUT_H

#include "common.h"
#include "scanner.h"

// Forward declaration so paths can hold children
struct SignatureNode;

// ==========================================
// THE OVERLOAD FOREST
// ==========================================

// 1. THE GUESS: Represents a specific path we can take based on arity
typedef struct OverloadPath {
  int segmentArity;  // How many args to parse right now? (e.g., 0 or 1)
  bool isTerminal;   // Does the function end here?
  char *mangledName; // The VM name: "move$1_to$1"

  // What words can come NEXT if we take this path?
  int childCount;
  int childCapacity;
  struct SignatureNode **children;
} OverloadPath;

// 2. THE WORD: The physical label in the code (e.g., "greet")
typedef struct SignatureNode {
  uint32_t hash; // Hash of the token label

  // The list of all valid guesses (arities) for this word
  int pathCount;
  int pathCapacity;
  OverloadPath *paths;
} SignatureNode;

// ==========================================
// GLOBALS EXPOSED TO THE COMPILER
// ==========================================

extern Token *tokenStream;
extern int tokenCount;
extern int tokenCapacity;
extern int currentTokenIndex;

// The root of the Overload Forest
extern SignatureNode *signatureTrieRoot;

// ==========================================
// PUBLIC API
// ==========================================

void initScout(const char *source);
void freeScout();
void hoistSignatures();
bool isLabelToken(Token *token);

// --- NEW NATIVE INJECTOR ---
void injectNativeSignature(const char *mangled);

#endif
