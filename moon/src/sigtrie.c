#include "sigtrie.h"
#include "object.h" // For hashString
#include <stdlib.h>
#include <string.h>

// --- MEMORY MACROS (Isolated from the VM) ---
#define ALLOCATE_TRIE(type, count) (type *)malloc(sizeof(type) * (count))
#define FREE_TRIE(ptr) free(ptr)

// --- THE HASH TABLE ---
typedef struct {
  uint32_t rootHash;
  char *rootWord;
  TrieNode *trieRoot;
} RegistryEntry;

#define TABLE_CAPACITY 1024
static RegistryEntry phrasalTable[TABLE_CAPACITY];

// --- UTILS ---
static char *my_strdup(const char *s) {
  size_t len = strlen(s) + 1;
  char *dup = malloc(len);
  if (dup)
    memcpy(dup, s, len);
  return dup;
}

// --- NODE MANAGEMENT ---
static TrieNode *newNode(PhraseNodeType type) {
  TrieNode *node = ALLOCATE_TRIE(TrieNode, 1);
  node->type = type;
  node->labelHash = 0;
  node->arity = 0;
  node->isTerminal = false;
  node->mangledName = NULL;
  node->children = NULL;
  node->childCount = 0;
  node->childCapacity = 0;
  return node;
}

static void freeTrieNode(TrieNode *node) {
  if (node == NULL)
    return;
  for (int i = 0; i < node->childCount; i++) {
    freeTrieNode(node->children[i]);
  }
  if (node->children != NULL)
    FREE_TRIE(node->children);
  if (node->mangledName != NULL)
    FREE_TRIE(node->mangledName);
  FREE_TRIE(node);
}

// --- PUBLIC API ---

void initSignatureTable() {
  for (int i = 0; i < TABLE_CAPACITY; i++) {
    phrasalTable[i].rootWord = NULL; // NULL means empty bucket
  }
}

void freeSignatureTable() {
  for (int i = 0; i < TABLE_CAPACITY; i++) {
    if (phrasalTable[i].rootWord != NULL) {
      FREE_TRIE(phrasalTable[i].rootWord);
      freeTrieNode(phrasalTable[i].trieRoot);
      phrasalTable[i].rootWord = NULL;
    }
  }
}

TrieNode *getSignatureTrie(const char *rootWord) {
  uint32_t hash = hashString(rootWord, strlen(rootWord));
  uint32_t index = hash & (TABLE_CAPACITY - 1);

  // O(1) Lookup with Linear Probing for collisions!
  for (;;) {
    RegistryEntry *entry = &phrasalTable[index];
    if (entry->rootWord == NULL)
      return NULL; // Not found
    if (entry->rootHash == hash && strcmp(entry->rootWord, rootWord) == 0) {
      return entry->trieRoot; // Found it!
    }
    index = (index + 1) & (TABLE_CAPACITY - 1);
  }
}

TrieNode *startPhrase(const char *rootWord, int length) {
  uint32_t rHash = hashString(rootWord, length);
  uint32_t index = rHash & (TABLE_CAPACITY - 1);

  for (;;) {
    RegistryEntry *entry = &phrasalTable[index];
    if (entry->rootWord == NULL) {
      entry->rootHash = rHash;
      char *rootStr = malloc(length + 1);
      memcpy(rootStr, rootWord, length);
      rootStr[length] = '\0';
      entry->rootWord = rootStr;

      entry->trieRoot = newNode(NODE_LABEL);
      return entry->trieRoot;
    } else if (entry->rootHash == rHash &&
               strncmp(entry->rootWord, rootWord, length) == 0 &&
               entry->rootWord[length] == '\0') {
      return entry->trieRoot;
    }
    index = (index + 1) & (TABLE_CAPACITY - 1);
  }
}

TrieNode *addLabelBranch(TrieNode *current, const char *label, int length) {
  uint32_t lHash = hashString(label, length);

  for (int i = 0; i < current->childCount; i++) {
    TrieNode *child = current->children[i];
    if (child->type == NODE_LABEL && child->labelHash == lHash)
      return child;
  }

  TrieNode *child = newNode(NODE_LABEL);
  child->labelHash = lHash;

  if (current->childCapacity < current->childCount + 1) {
    int old = current->childCapacity;
    current->childCapacity = old < 4 ? 4 : old * 2;
    current->children = (TrieNode **)realloc(
        current->children, sizeof(TrieNode *) * current->childCapacity);
  }
  current->children[current->childCount++] = child;
  return child;
}

TrieNode *addArgumentBranch(TrieNode *current, int arity) {
  for (int i = 0; i < current->childCount; i++) {
    TrieNode *child = current->children[i];
    if (child->type == NODE_ARGUMENT && child->arity == arity)
      return child;
  }

  TrieNode *child = newNode(NODE_ARGUMENT);
  child->arity = arity;

  if (current->childCapacity < current->childCount + 1) {
    int old = current->childCapacity;
    current->childCapacity = old < 4 ? 4 : old * 2;
    current->children = (TrieNode **)realloc(
        current->children, sizeof(TrieNode *) * current->childCapacity);
  }
  current->children[current->childCount++] = child;
  return child;
}

void finalizePhrase(TrieNode *endNode, const char *mangledName) {
  endNode->isTerminal = true;
  if (endNode->mangledName)
    free(endNode->mangledName);
  endNode->mangledName = my_strdup(mangledName);
}
