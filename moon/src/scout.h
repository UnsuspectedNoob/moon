#ifndef MOON_SCOUT_H
#define MOON_SCOUT_H

#include "common.h"
#include "scanner.h"

// ==========================================
// PHRASAL FUNCTION TRIE
// ==========================================

typedef struct TrieNode {
  uint32_t hash;      // The hashed token label (e.g., hash("from"))
  int segmentArity;   // Args immediately following this word
  int totalArity;     // Total args for the final VM call
  bool isTerminal;    // True if this ends a valid phrase
  char *stitchedName; // The name the VM sees: "move_from_to"

  // Dynamic array of child nodes
  int childCount;
  int childCapacity;
  struct TrieNode **children;
} TrieNode;

// ==========================================
// GLOBALS EXPOSED TO THE COMPILER
// ==========================================

// The Token Cache
extern Token *tokenStream;
extern int tokenCount;
extern int tokenCapacity;
extern int currentTokenIndex;

// The Trie States
extern TrieNode *signatureTrieRoot;
extern TrieNode *activePhraseNode;

// ==========================================
// PUBLIC API
// ==========================================

// Initializes the scanner, fills the Token Cache, and prepares the Root Trie
// Node
void initScout(const char *source);

// Cleans up the Token Cache and frees the entire Trie from memory
void freeScout();

// The core Pass 0 function that maps out the signatures
void hoistSignatures();

// Helper to check if a token is a valid phrasal label (Identifier or Keyword)
bool isLabelToken(Token *token);

#endif
