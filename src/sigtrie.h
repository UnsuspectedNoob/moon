#ifndef moon_sigtrie_h
#define moon_sigtrie_h

#include <stdbool.h>
#include <stdint.h>

// 1. Node Types
typedef enum {
  NODE_LABEL,   // e.g., "of", "and"
  NODE_ARGUMENT // e.g., $1, $2
} PhraseNodeType;

// 2. The Trie Node (The DFA State)
typedef struct TrieNode {
  PhraseNodeType type;

  // Payload
  uint32_t labelHash; // Used if type == NODE_LABEL
  char *labelName;    // Stored string for debugging/printing
  int arity;          // Used if type == NODE_ARGUMENT

  // Accept State
  bool isTerminal;
  bool isCore;
  char *mangledName;

  // Branches
  struct TrieNode **children;
  int childCount;
  int childCapacity;
} TrieNode;

// 3. The Public API
void initSignatureTable();
void freeSignatureTable();
void printSignatureTrie();
TrieNode *getSignatureTrie(const char *rootWord);

// --- THE DIRECT BUILDER API ---
TrieNode *startPhrase(const char *rootWord, int length);
TrieNode *addLabelBranch(TrieNode *current, const char *label, int length);
TrieNode *addArgumentBranch(TrieNode *current, int arity);
void finalizePhrase(TrieNode *endNode, const char *mangledName);

// Used by the Parser to fetch the tree
TrieNode *getSignatureTrie(const char *rootWord);

// Used by the Static Linker to register C-level natives
void registerSignature(const char *root, const char *path, const char *mangledName);

#endif
