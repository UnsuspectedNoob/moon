#ifndef moon_sigtrie_h
#define moon_sigtrie_h

#include <stdbool.h>
#include <stdint.h>

// 1. Node Types
typedef enum {
  NODE_LABEL,   // e.g., "of", "and"
  NODE_ARGUMENT // e.g., $1, $2
} PhraseNodeType;

typedef enum {
  TERMINAL_NONE,
  TERMINAL_PHRASE,
  TERMINAL_VARIABLE
} TerminalType;

// 2. The Trie Node (The DFA State)
typedef struct TrieNode {
  PhraseNodeType type;

  // Payload
  uint32_t labelHash; // Used if type == NODE_LABEL
  char *labelName;    // Stored string for debugging/printing
  int labelLength;
  int arity;          // Used if type == NODE_ARGUMENT
  bool isLeadingArg;  // True if this argument appears before root word (infix)

  // Accept State
  TerminalType terminalType;
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
bool hasInfixSignature(const char *word, int length);

TrieNode *startPhrase(const char *rootWord, int length);
TrieNode *addLabelBranch(TrieNode *current, const char *label, int length);
TrieNode *addArgumentBranch(TrieNode *current, int arity, bool isLeadingArg);
bool finalizePhrase(TrieNode *endNode, const char *mangledName, TerminalType terminalType);

// Used by the Static Linker to register C-level natives
void registerSignature(const char *root, const char *path, const char *mangledName);

#endif
