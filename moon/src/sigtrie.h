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
  int arity;          // Used if type == NODE_ARGUMENT

  // Accept State
  bool isTerminal;
  char *mangledName;

  // Branches
  struct TrieNode **children;
  int childCount;
  int childCapacity;
} TrieNode;

// 3. The Public API
void initSignatureTable();
void freeSignatureTable();
TrieNode *getSignatureTrie(const char *rootWord);

// --- THE DIRECT BUILDER API ---
TrieNode *startPhrase(const char *rootWord, int length);
TrieNode *addLabelBranch(TrieNode *current, const char *label, int length);
TrieNode *addArgumentBranch(TrieNode *current, int arity);
void finalizePhrase(TrieNode *endNode, const char *mangledName);

// Used by the Pre-Pass Skimmer to build the tree
void insertSignature(const char *rootWord, const char *mangledName);

// Used by the Parser to fetch the tree
TrieNode *getSignatureTrie(const char *rootWord);

#endif
