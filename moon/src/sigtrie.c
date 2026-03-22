#include "sigtrie.h"
#include "object.h"
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

#define TABLE_MAX 255
static RegistryEntry phrasalTable[TABLE_MAX];
static int tableCount = 0;

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

static void freeNode(TrieNode *node) {
  if (node == NULL)
    return;
  for (int i = 0; i < node->childCount; i++) {
    freeNode(node->children[i]);
  }
  if (node->children != NULL)
    FREE_TRIE(node->children);
  if (node->mangledName != NULL)
    FREE_TRIE(node->mangledName);
  FREE_TRIE(node);
}

// --- PUBLIC API ---

void initSignatureTable() {
  tableCount = 0;
  for (int i = 0; i < TABLE_MAX; i++) {
    phrasalTable[i].rootHash = 0;
    phrasalTable[i].rootWord = NULL;
    phrasalTable[i].trieRoot = NULL;
  }
}

void freeSignatureTable() {
  for (int i = 0; i < tableCount; i++) {
    FREE_TRIE(phrasalTable[i].rootWord);
    freeNode(phrasalTable[i].trieRoot);
  }
  tableCount = 0;
}

TrieNode *getSignatureTrie(const char *rootWord) {
  uint32_t hash = hashString(rootWord, strlen(rootWord));
  for (int i = 0; i < tableCount; i++) {
    if (phrasalTable[i].rootHash == hash) {
      return phrasalTable[i].trieRoot;
    }
  }
  return NULL;
}

void insertSignature(const char *rootWord, const char *mangledName) {
  // 1. Find or create the root entry in the Hash Table
  uint32_t rHash = hashString(rootWord, strlen(rootWord));
  TrieNode *current = NULL;

  for (int i = 0; i < tableCount; i++) {
    if (phrasalTable[i].rootHash == rHash) {
      current = phrasalTable[i].trieRoot;
      break;
    }
  }

  if (current == NULL) {
    if (tableCount >= TABLE_MAX)
      return; // Table full
    phrasalTable[tableCount].rootHash = rHash;
    phrasalTable[tableCount].rootWord = my_strdup(rootWord);
    phrasalTable[tableCount].trieRoot = newNode(NODE_LABEL); // Dummy root
    current = phrasalTable[tableCount].trieRoot;
    tableCount++;
  }

  // 2. Walk the mangled string and build the DFA branches
  const char *cursor = mangledName + strlen(rootWord);

  // If there is nothing after the root (e.g., "greet$0")
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

  // Reset cursor to parse actual branches
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

    // Do we already have this child?
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

    // If not, create it!
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

    current = nextNode; // Move down the tree
  }

  // 3. Mark the finish line
  current->isTerminal = true;
  if (current->mangledName)
    FREE_TRIE(current->mangledName);
  current->mangledName = my_strdup(mangledName);
}
