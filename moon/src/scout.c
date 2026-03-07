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

TrieNode *signatureTrieRoot = NULL;
TrieNode *activePhraseNode = NULL;

// ==========================================
// INTERNAL HELPERS
// ==========================================

static TrieNode *newTrieNode(uint32_t hash) {
  TrieNode *node = ALLOCATE(TrieNode, 1);

  node->hash = hash;
  node->segmentArity = 0;
  node->totalArity = 0;
  node->isTerminal = false;
  node->stitchedName = NULL;
  node->childCount = 0;
  node->childCapacity = 0;
  node->children = NULL;
  return node;
}

static void freeTrie(TrieNode *node) {
  if (node == NULL)
    return;

  for (int i = 0; i < node->childCount; i++) {
    freeTrie(node->children[i]);
  }

  FREE_ARRAY(TrieNode *, node->children, node->childCapacity);

  if (node->stitchedName != NULL) {
    free(node->stitchedName);
  }

  FREE(TrieNode, node);
}

// ==========================================
// PUBLIC API IMPLEMENTATION
// ==========================================

void initScout(const char *source) {
  initScanner(source);

  // 1. FILL THE TOKEN CACHE (O(N) single pass lexing)
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

  // 2. INITIALIZE TRIE STATE
  signatureTrieRoot = newTrieNode(0);
  activePhraseNode = NULL;
  currentTokenIndex = 0;
}

void freeScout() {
  freeTrie(signatureTrieRoot);
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
        int totalArity = 0;

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
            totalArity += arity;
            segmentCount++;

          } else {
            cursor++;
          }
        }

        if (segmentCount > 0) {
          int nameLen = 0;
          for (int s = 0; s < segmentCount; s++) {
            nameLen += segmentTokens[s].length;
            if (s < segmentCount - 1)
              nameLen++;
          }

          char *stitchedName = ALLOCATE(char, nameLen + 1);
          int offset = 0;
          for (int s = 0; s < segmentCount; s++) {
            memcpy(stitchedName + offset, segmentTokens[s].start,
                   segmentTokens[s].length);
            offset += segmentTokens[s].length;
            if (s < segmentCount - 1)
              stitchedName[offset++] = '_';
          }
          stitchedName[nameLen] = '\0';

          TrieNode *currentTrie = signatureTrieRoot;
          for (int s = 0; s < segmentCount; s++) {
            TrieNode *nextNode = NULL;
            for (int c = 0; c < currentTrie->childCount; c++) {
              if (currentTrie->children[c]->hash == segmentHashes[s]) {
                nextNode = currentTrie->children[c];
                break;
              }
            }

            if (nextNode == NULL) {
              nextNode = newTrieNode(segmentHashes[s]);
              if (currentTrie->childCapacity < currentTrie->childCount + 1) {
                int oldCap = currentTrie->childCapacity;
                currentTrie->childCapacity = GROW_CAPACITY(oldCap);
                currentTrie->children =
                    GROW_ARRAY(TrieNode *, currentTrie->children, oldCap,
                               currentTrie->childCapacity);
              }
              currentTrie->children[currentTrie->childCount++] = nextNode;
            }

            nextNode->segmentArity = segmentArities[s];
            currentTrie = nextNode;
          }

          currentTrie->isTerminal = true;
          currentTrie->totalArity = totalArity;

          if (currentTrie->stitchedName != NULL) {
            FREE_ARRAY(char, currentTrie->stitchedName,
                       strlen(currentTrie->stitchedName) + 1);
          }
          currentTrie->stitchedName = stitchedName;
        }
      }
    }
  }
}
