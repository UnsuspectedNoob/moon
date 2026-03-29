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
  uint32_t index = hash % TABLE_CAPACITY;

  // O(1) Lookup with Linear Probing for collisions!
  for (;;) {
    RegistryEntry *entry = &phrasalTable[index];
    if (entry->rootWord == NULL)
      return NULL; // Not found
    if (entry->rootHash == hash && strcmp(entry->rootWord, rootWord) == 0) {
      return entry->trieRoot; // Found it!
    }
    index = (index + 1) % TABLE_CAPACITY;
  }
}

void insertSignature(const char *rootWord, const char *mangledName) {
  uint32_t rHash = hashString(rootWord, strlen(rootWord));
  uint32_t index = rHash % TABLE_CAPACITY;
  TrieNode *current = NULL;

  // Find the exact bucket or an empty one
  for (;;) {
    RegistryEntry *entry = &phrasalTable[index];
    if (entry->rootWord == NULL) {
      // Empty bucket, claim it!
      entry->rootHash = rHash;
      entry->rootWord = my_strdup(rootWord);
      entry->trieRoot = newNode(NODE_LABEL);
      current = entry->trieRoot;
      break;
    } else if (entry->rootHash == rHash &&
               strcmp(entry->rootWord, rootWord) == 0) {
      // Found existing root!
      current = entry->trieRoot;
      break;
    }
    index = (index + 1) % TABLE_CAPACITY;
  }

  // 2. Walk the mangled string and build the DFA branches
  const char *cursor = mangledName + strlen(rootWord);

  if (*cursor == '$') {
    cursor++;
    int arity = atoi(cursor);
    if (arity == 0) {
      current->isTerminal = true;
      if (current->mangledName)
        FREE_TRIE(current->mangledName);
      current->mangledName = my_strdup(mangledName);
      return;
    }
  }

  cursor = mangledName + strlen(rootWord);

  while (*cursor != '\0') {
    PhraseNodeType nextType;
    uint32_t nextHash = 0;
    int nextArity = 0;

    if (*cursor == '_') {
      cursor++;
      nextType = NODE_LABEL;

      char labelBuf[256] = {0};
      int len = 0;
      while (cursor[len] != '\0' && cursor[len] != '$' && cursor[len] != '_')
        len++;
      strncpy(labelBuf, cursor, len);

      nextHash = hashString(labelBuf, len);
      cursor += len;
    } else if (*cursor == '$') {
      cursor++;
      nextType = NODE_ARGUMENT;
      nextArity = atoi(cursor);
      while (*cursor >= '0' && *cursor <= '9')
        cursor++;
    } else {
      cursor++;
      continue;
    }

    TrieNode *nextNode = NULL;
    for (int i = 0; i < current->childCount; i++) {
      TrieNode *child = current->children[i];
      if (child->type == nextType) {
        if (nextType == NODE_LABEL && child->labelHash == nextHash) {
          nextNode = child;
          break;
        }
        if (nextType == NODE_ARGUMENT && child->arity == nextArity) {
          nextNode = child;
          break;
        }
      }
    }

    if (nextNode == NULL) {
      nextNode = newNode(nextType);
      nextNode->labelHash = nextHash;
      nextNode->arity = nextArity;

      if (current->childCapacity < current->childCount + 1) {
        int old = current->childCapacity;
        current->childCapacity = old < 4 ? 4 : old * 2;
        current->children = (TrieNode **)realloc(
            current->children, sizeof(TrieNode *) * current->childCapacity);
      }
      current->children[current->childCount++] = nextNode;
    }

    current = nextNode;
  }

  current->isTerminal = true;
  if (current->mangledName)
    FREE_TRIE(current->mangledName);
  current->mangledName = my_strdup(mangledName);
}
