#ifndef MOON_PARSER_H
#define MOON_PARSER_H

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

  // --- PHASE 3: SPECULATIVE PARSING FLAGS ---
  bool isSpeculating;     // Are we currently guessing an overload path?
  bool speculationFailed; // Did the guess hit a syntax error?
} Parser;

// The global parser state
extern Parser parser;

// ==========================================
// ERROR HANDLING
// ==========================================

void errorAt(Token *token, const char *message);
void error(const char *message);
void errorAtCurrent(const char *message);
void synchronize();

// ==========================================
// TOKEN ADVANCEMENT & CHECKING
// ==========================================

void advance();
void consume(TokenType type, const char *message);
bool check(TokenType type);
bool checkTerminator(TokenType *terminators, int count);
bool match(TokenType type);
void ignoreNewlines();
void consumeEnd();

#endif
