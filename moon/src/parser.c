#include <stdio.h>

#include "parser.h"
#include "scout.h" // Gives us access to tokenStream and currentTokenIndex

// ==========================================
// GLOBAL STATE
// ==========================================

Parser parser;

// ==========================================
// ERROR HANDLING
// ==========================================

void errorAt(Token *token, const char *message) {
  if (parser.panicMode)
    return;

  parser.panicMode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

void error(const char *message) { errorAt(&parser.previous, message); }

void errorAtCurrent(const char *message) { errorAt(&parser.current, message); }

// Error synchronization (skips tokens until a safe point to resume parsing)
void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_END)
      return;

    switch (parser.current.type) {
    case TOKEN_LET:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_SHOW:
    case TOKEN_GIVE:
    case TOKEN_SET:
      return;
    default:;
    }

    advance();
  }
}

// ==========================================
// TOKEN ADVANCEMENT & CHECKING
// ==========================================

void advance() {
  parser.previous = parser.current;

  for (;;) {
    // Pull the next token from our pre-lexed array!
    parser.current = tokenStream[currentTokenIndex++];

    if (parser.current.type != TOKEN_ERROR)
      break;

    errorAtCurrent(parser.current.start);
  }
}

void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

bool check(TokenType type) { return parser.current.type == type; }

bool checkTerminator(TokenType *terminators, int count) {
  for (int i = 0; i < count; i++) {
    if (check(terminators[i]))
      return true;
  }
  return check(TOKEN_EOF); // Always stop at EOF to prevent infinite loops
}

bool match(TokenType type) {
  if (!check(type))
    return false;
  advance();
  return true;
}

void ignoreNewlines() {
  while (match(TOKEN_NEWLINE)) {
    // Just consume them and do nothing.
  }
}

void consumeEnd() {
  // 1. If we hit a newline, eat it and we are good.
  if (match(TOKEN_NEWLINE))
    return;

  // 2. If we hit EOF, we are good.
  if (check(TOKEN_EOF))
    return;

  // 3. Soft Terminators: These tokens imply the current statement is over,
  //    so we STOP parsing here, but we do NOT consume them.
  //    (The parent syntax, like 'if', will consume the 'else')
  if (check(TOKEN_ELSE) || check(TOKEN_END) || check(TOKEN_RIGHT_BRACE))
    return;

  // 4. If none of the above, THEN we complain.
  errorAtCurrent("Expect newline or end of statement.");
}
