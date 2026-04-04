#ifndef MOON_PARSER_H
#define MOON_PARSER_H

#include "ast.h" // We need this so it knows what a Node is!
#include "common.h"
#include "error.h"
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
void errorAt(Token *token, ErrorType type, const char *message,
             const char *hint);
void error(const char *message);
void consumeHint(TokenType type, ErrorType errType, const char *message,
                 const char *hint);
void consume(TokenType type, const char *message);
void synchronize();

void advance();
bool check(TokenType type);
bool checkTerminator(TokenType *terminators, int count);
bool match(TokenType type);
void ignoreNewlines();

#endif
