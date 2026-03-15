#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "scout.h"

// ==========================================
// GLOBAL DEFINITIONS
// ==========================================

Token *tokenStream = NULL;
int tokenCount = 0;
int tokenCapacity = 0;
int currentTokenIndex = 0;

SignatureNode *signatureTrieRoot = NULL;

// ==========================================
// INTERNAL HELPERS (Memory Management)
// ==========================================

static SignatureNode *newSignatureNode(uint32_t hash) {
  SignatureNode *node = ALLOCATE(SignatureNode, 1);
  node->hash = hash;
  node->pathCount = 0;
  node->pathCapacity = 0;
  node->paths = NULL;
  return node;
}

static void initOverloadPath(OverloadPath *path, int arity) {
  path->segmentArity = arity;
  path->isTerminal = false;
  path->mangledName = NULL;
  path->childCount = 0;
  path->childCapacity = 0;
  path->children = NULL;
}

static void freeSignatureNode(SignatureNode *node) {
  if (node == NULL)
    return;

  for (int p = 0; p < node->pathCount; p++) {
    OverloadPath *path = &node->paths[p];

    // Free all children down this path
    for (int c = 0; c < path->childCount; c++) {
      freeSignatureNode(path->children[c]);
    }
    FREE_ARRAY(SignatureNode *, path->children, path->childCapacity);

    // Free the mangled name
    if (path->mangledName != NULL) {
      FREE_ARRAY(char, path->mangledName, strlen(path->mangledName) + 1);
    }
  }

  FREE_ARRAY(OverloadPath, node->paths, node->pathCapacity);
  FREE(SignatureNode, node);
}

// ==========================================
// PUBLIC API IMPLEMENTATION
// ==========================================

void initScout(const char *source) {
  initScanner(source);

  tokenCount = 0;
  tokenCapacity = 0;
  tokenStream = NULL;

  for (;;) {
    Token token = scanToken();

    if (tokenCapacity < tokenCount + 1) {
      int oldCapacity = tokenCapacity;
      tokenCapacity = GROW_CAPACITY(oldCapacity);
      tokenStream = GROW_ARRAY(Token, tokenStream, oldCapacity, tokenCapacity);
    }

    tokenStream[tokenCount++] = token;
    if (token.type == TOKEN_EOF)
      break;
  }

  // Set up the Root Node. It represents the "start" of a phrase.
  // We give it exactly ONE path (arity 0) that holds all starting words.
  signatureTrieRoot = newSignatureNode(0);
  signatureTrieRoot->pathCapacity = 1;
  signatureTrieRoot->paths = ALLOCATE(OverloadPath, 1);
  initOverloadPath(&signatureTrieRoot->paths[0], 0);
  signatureTrieRoot->pathCount = 1;

  injectNativeSignature("add$1_to$1");

  currentTokenIndex = 0;
}

// ========================================================
// THE C-LEVEL NATIVE INJECTOR
// Takes a mangled name like "add$1_to$1" and forces it into the Trie
// ========================================================
void injectNativeSignature(const char *mangled) {
  if (signatureTrieRoot == NULL)
    return;

  OverloadPath *currentPath = &signatureTrieRoot->paths[0];

  char buffer[256];
  strcpy(buffer, mangled);

  // Split by '_' to get each segment (e.g., "add$1")
  char *segment = strtok(buffer, "_");
  while (segment != NULL) {
    char *split = strchr(segment, '$');
    if (!split)
      break;

    // Extract the word
    int wordLen = split - segment;
    char word[256];
    strncpy(word, segment, wordLen);
    word[wordLen] = '\0';

    // Extract the arity
    int arity = atoi(split + 1);

    uint32_t hash = hashString(word, wordLen);

    // 1. Find or create the SignatureNode for the word
    SignatureNode *wordNode = NULL;
    for (int c = 0; c < currentPath->childCount; c++) {
      if (currentPath->children[c]->hash == hash) {
        wordNode = currentPath->children[c];
        break;
      }
    }
    if (wordNode == NULL) {
      wordNode = newSignatureNode(hash);
      if (currentPath->childCapacity < currentPath->childCount + 1) {
        int oldCap = currentPath->childCapacity;
        currentPath->childCapacity = GROW_CAPACITY(oldCap);
        currentPath->children =
            GROW_ARRAY(SignatureNode *, currentPath->children, oldCap,
                       currentPath->childCapacity);
      }
      currentPath->children[currentPath->childCount++] = wordNode;
    }

    // 2. Find or create the OverloadPath for the arity
    OverloadPath *nextPath = NULL;
    for (int p = 0; p < wordNode->pathCount; p++) {
      if (wordNode->paths[p].segmentArity == arity) {
        nextPath = &wordNode->paths[p];
        break;
      }
    }
    if (nextPath == NULL) {
      if (wordNode->pathCapacity < wordNode->pathCount + 1) {
        int oldCap = wordNode->pathCapacity;
        wordNode->pathCapacity = GROW_CAPACITY(oldCap);
        wordNode->paths = GROW_ARRAY(OverloadPath, wordNode->paths, oldCap,
                                     wordNode->pathCapacity);
      }
      nextPath = &wordNode->paths[wordNode->pathCount++];
      initOverloadPath(nextPath, arity);
    }

    currentPath = nextPath;
    segment = strtok(NULL, "_");
  }

  // 3. Mark the end of the chain as Terminal!
  currentPath->isTerminal = true;
  int exactLength = strlen(mangled);
  char *exactName = ALLOCATE(char, exactLength + 1);
  strcpy(exactName, mangled);
  currentPath->mangledName = exactName;
}

void freeScout() {
  freeSignatureNode(signatureTrieRoot);
  FREE_ARRAY(Token, tokenStream, tokenCapacity);

  tokenStream = NULL;
  tokenCount = 0;
  tokenCapacity = 0;
}

bool isLabelToken(Token *token) {
  if (token->type == TOKEN_EOF || token->length == 0)
    return false;
  char c = token->start[0];
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

void hoistSignatures() {
  for (int i = 0; i < tokenCount; i++) {
    if (tokenStream[i].type == TOKEN_LET) {
      int peek = i + 1;
      bool isFunction = false;

      while (peek < tokenCount && tokenStream[peek].type != TOKEN_NEWLINE &&
             tokenStream[peek].type != TOKEN_BE &&
             tokenStream[peek].type != TOKEN_EOF) {
        if (tokenStream[peek].type == TOKEN_COLON) {
          isFunction = true;
          break;
        }
        peek++;
      }

      if (isFunction) {
        int cursor = i + 1;
#define MAX_SEGMENTS 16
        uint32_t segmentHashes[MAX_SEGMENTS];
        int segmentArities[MAX_SEGMENTS];
        Token segmentTokens[MAX_SEGMENTS];

        int segmentCount = 0;

        while (cursor < peek) {
          if (isLabelToken(&tokenStream[cursor])) {
            segmentTokens[segmentCount] = tokenStream[cursor];
            segmentHashes[segmentCount] = hashString(
                tokenStream[cursor].start, tokenStream[cursor].length);
            int arity = 0;
            cursor++;

            if (cursor < peek && tokenStream[cursor].type == TOKEN_LEFT_PAREN) {
              cursor++;
              while (cursor < peek &&
                     tokenStream[cursor].type != TOKEN_RIGHT_PAREN) {
                if (isLabelToken(&tokenStream[cursor])) {
                  arity++;
                }
                cursor++;
              }
              if (cursor < peek &&
                  tokenStream[cursor].type == TOKEN_RIGHT_PAREN) {
                cursor++;
              }
            }

            segmentArities[segmentCount] = arity;
            segmentCount++;

          } else {
            cursor++;
          }
        }

        if (segmentCount > 0) {
          // ========================================================
          // PHASE 2A: NAME MANGLING (e.g. "move$1_to$1")
          // ========================================================
          int nameLen = 0;
          for (int s = 0; s < segmentCount; s++) {
            // Buffer size: Token length + up to 3 chars for arity + 1 for '$' +
            // 1 for '_'
            nameLen += segmentTokens[s].length + 5;
          }

          char *mangledName = ALLOCATE(char, nameLen);
          mangledName[0] = '\0'; // Start empty for strcat

          for (int s = 0; s < segmentCount; s++) {
            char tempWord[256];
            memcpy(tempWord, segmentTokens[s].start, segmentTokens[s].length);
            tempWord[segmentTokens[s].length] = '\0';

            char segmentBuffer[512];
            if (s < segmentCount - 1) {
              sprintf(segmentBuffer, "%s$%d_", tempWord, segmentArities[s]);
            } else {
              sprintf(segmentBuffer, "%s$%d", tempWord, segmentArities[s]);
            }
            strcat(mangledName, segmentBuffer);
          }

          // ========================================================
          // PHASE 2B: WEAVING THE OVERLOAD FOREST
          // ========================================================
          // We always start at the single path inside the Root Node
          OverloadPath *currentPath = &signatureTrieRoot->paths[0];

          for (int s = 0; s < segmentCount; s++) {
            SignatureNode *wordNode = NULL;

            // 1. Check if the word exists in the CURRENT path's children
            for (int c = 0; c < currentPath->childCount; c++) {
              if (currentPath->children[c]->hash == segmentHashes[s]) {
                wordNode = currentPath->children[c];
                break;
              }
            }

            // 2. If the word doesn't exist, create it!
            if (wordNode == NULL) {
              wordNode = newSignatureNode(segmentHashes[s]);
              if (currentPath->childCapacity < currentPath->childCount + 1) {
                int oldCap = currentPath->childCapacity;
                currentPath->childCapacity = GROW_CAPACITY(oldCap);
                currentPath->children =
                    GROW_ARRAY(SignatureNode *, currentPath->children, oldCap,
                               currentPath->childCapacity);
              }
              currentPath->children[currentPath->childCount++] = wordNode;
            }

            // 3. Look for the correct ARITY GUESS inside this word
            OverloadPath *nextPath = NULL;
            for (int p = 0; p < wordNode->pathCount; p++) {
              if (wordNode->paths[p].segmentArity == segmentArities[s]) {
                nextPath = &wordNode->paths[p];
                break;
              }
            }

            // 4. If this word has never seen this arity guess, create it!
            if (nextPath == NULL) {
              if (wordNode->pathCapacity < wordNode->pathCount + 1) {
                int oldCap = wordNode->pathCapacity;
                wordNode->pathCapacity = GROW_CAPACITY(oldCap);
                wordNode->paths = GROW_ARRAY(OverloadPath, wordNode->paths,
                                             oldCap, wordNode->pathCapacity);
              }
              nextPath = &wordNode->paths[wordNode->pathCount++];
              initOverloadPath(nextPath, segmentArities[s]);
            }

            // 5. Move down the tree!
            currentPath = nextPath;
          }

          // Mark the absolute final path as Terminal and save the mangled name!
          currentPath->isTerminal = true;

          if (currentPath->mangledName != NULL) {
            FREE_ARRAY(char, currentPath->mangledName,
                       strlen(currentPath->mangledName) + 1);
          }
          // The buffer size was 'nameLen', but we must allocate exactly the
          // final string length + 1
          int exactLength = strlen(mangledName);
          char *exactName = ALLOCATE(char, exactLength + 1);
          strcpy(exactName, mangledName);
          FREE_ARRAY(char, mangledName, nameLen); // Free the generous buffer

          currentPath->mangledName = exactName;
        }
      }
    }
  }
}
