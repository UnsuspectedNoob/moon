#ifndef MOON_PARSER_H
#define MOON_PARSER_H

#include "ast.h" // We need this so it knows what a Node is!
#include "common.h"
#include "scanner.h"

// ==========================================
// PARSER STATE
// ==========================================
typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

extern Parser parser;

// --- Expose the new AST Builder! ---

void hoistPhrases(const char *source);
Node *parseSource(const char *source);

// ==========================================
// ERROR HANDLING & TOKEN ADVANCEMENT
// ==========================================
void errorAt(Token *token, const char *message);
void error(const char *message);
void errorAtCurrent(const char *message);
void synchronize();

void advance();
void consume(TokenType type, const char *message);
bool check(TokenType type);
bool checkTerminator(TokenType *terminators, int count);
bool match(TokenType type);
void ignoreNewlines();
void consumeEnd();

#endif
