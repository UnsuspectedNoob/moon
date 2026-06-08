#include "sigtrie.h"
#include "object.h" // For hashString
#include "vm.h"     // For g_isBootstrappingCore
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
  node->isCore = g_isBootstrappingCore;
  node->labelName = NULL;
  node->mangledName = NULL;
  node->children = NULL;
  node->childCount = 0;
  node->childCapacity = 0;
  return node;
}

static void destroyTrieNode(TrieNode *node) {
  if (node == NULL)
    return;
  for (int i = 0; i < node->childCount; i++) {
    destroyTrieNode(node->children[i]);
  }
  if (node->children != NULL)
    FREE_TRIE(node->children);
  if (node->labelName != NULL)
    FREE_TRIE(node->labelName);
  if (node->mangledName != NULL)
    FREE_TRIE(node->mangledName);
  FREE_TRIE(node);
}

static void freeTrieNode(TrieNode *node) {
  if (node == NULL)
    return;

  int newChildCount = 0;
  for (int i = 0; i < node->childCount; i++) {
    TrieNode *child = node->children[i];
    if (child->isCore) {
      node->children[newChildCount++] = child; // Keep it
      freeTrieNode(child); // Clean up its non-core descendants
    } else {
      destroyTrieNode(child); // Completely destroy this non-core branch
    }
  }
  node->childCount = newChildCount;
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
      if (phrasalTable[i].trieRoot->isCore) {
        freeTrieNode(phrasalTable[i].trieRoot);
        continue;
      }
      FREE_TRIE(phrasalTable[i].rootWord);
      destroyTrieNode(phrasalTable[i].trieRoot);
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
  child->labelName = malloc(length + 1);
  memcpy(child->labelName, label, length);
  child->labelName[length] = '\0';

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

static void printTrieNode(TrieNode *node, int indent, bool isLast) {
  for (int i = 0; i < indent - 1; i++) {
    printf("│   ");
  }
  if (indent > 0) {
    if (isLast) printf("└─ ");
    else printf("├─ ");
  }

  if (node->type == NODE_LABEL) {
    printf("[LABEL: %s]", node->labelName ? node->labelName : "?");
  } else if (node->type == NODE_ARGUMENT) {
    printf("[ARG: $%d]", node->arity);
  }

  if (node->isTerminal) {
    printf(" -> %s", node->mangledName);
  }
  printf("\n");

  for (int i = 0; i < node->childCount; i++) {
    printTrieNode(node->children[i], indent + 1, i == node->childCount - 1);
  }
}

void printSignatureTrie() {
  printf("=== SIGNATURE TRIE ===\n");
  int bucketCount = 0;
  for (int i = 0; i < TABLE_CAPACITY; i++) {
    if (phrasalTable[i].rootWord != NULL) {
      bucketCount++;
      printf("[ROOT: %s]", phrasalTable[i].rootWord);
      if (phrasalTable[i].trieRoot->isTerminal) {
        printf(" -> %s\n", phrasalTable[i].trieRoot->mangledName);
      } else {
        printf("\n");
      }
      for (int j = 0; j < phrasalTable[i].trieRoot->childCount; j++) {
        printTrieNode(phrasalTable[i].trieRoot->children[j], 1, j == phrasalTable[i].trieRoot->childCount - 1);
      }
    }
  }
  printf("======================\n");
  printf("Total Roots: %d\n\n", bucketCount);
}

void registerSignature(const char *root, const char *path,
                       const char *mangledName) {
  TrieNode *current = startPhrase(root, strlen(root));

  if (path == NULL || strlen(path) == 0) {
    finalizePhrase(current, mangledName);
    return;
  }

  char *pathCopy = my_strdup(path);
  char *token = strtok(pathCopy, ",");

  while (token != NULL) {
    if (token[0] == '$') {
      int arity = atoi(token + 1);
      current = addArgumentBranch(current, arity);
    } else {
      current = addLabelBranch(current, token, strlen(token));
    }
    token = strtok(NULL, ",");
  }

  finalizePhrase(current, mangledName);
  free(pathCopy);
}
