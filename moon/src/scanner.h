#ifndef MOON_SCANNER_H
#define MOON_SCANNER_H

typedef enum {
  // Single-character tokens
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE,
  TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_BRACKET,
  TOKEN_RIGHT_BRACKET,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_DOT,
  TOKEN_MINUS,
  TOKEN_MOD,
  TOKEN_PLUS,
  TOKEN_SLASH,
  TOKEN_STAR,
  TOKEN_ADD_INPLACE,

  // One or two character tokens
  TOKEN_BANG,
  TOKEN_BANG_EQUAL,
  TOKEN_EQUAL,
  TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER,
  TOKEN_GREATER_EQUAL,
  TOKEN_LESS,
  TOKEN_LESS_EQUAL,
  TOKEN_POSSESSIVE, // 's

  // Literals
  TOKEN_IDENTIFIER,
  TOKEN_STRING,        // "Hello"
  TOKEN_STRING_OPEN,   // "Hello `
  TOKEN_STRING_MIDDLE, // ` world `
  TOKEN_STRING_CLOSE,  // `!"
  TOKEN_NUMBER,

  // Keywords
  TOKEN_ADD,
  TOKEN_AND,
  TOKEN_AS,
  TOKEN_BE,
  TOKEN_BY,
  TOKEN_BREAK,
  TOKEN_EACH,
  TOKEN_ELSE,
  TOKEN_END,
  TOKEN_FALSE,
  TOKEN_FOR,
  TOKEN_FROM,
  TOKEN_GIVE,
  TOKEN_IF,
  TOKEN_IN,
  TOKEN_IS,
  TOKEN_LET,
  TOKEN_NIL,
  TOKEN_NOT,
  TOKEN_OR,
  TOKEN_QUIT,
  TOKEN_SET,
  TOKEN_SKIP,
  TOKEN_THEN,
  TOKEN_TO,
  TOKEN_TRUE,
  TOKEN_TYPE,
  TOKEN_UNLESS,
  TOKEN_UNTIL,
  TOKEN_UPDATE,
  TOKEN_WHILE,
  TOKEN_WITH,

  TOKEN_NEWLINE,
  TOKEN_ERROR,
  TOKEN_EOF
} TokenType;

typedef struct {
  TokenType type;
  const char *start; // Pointer to the start of the token in source
  int length;        // Length of the token
  int line;          // Line number for error reporting
  int column;        // Track horizontal positioning
} Token;

// Initialize the scanner with the source code string
void initScanner(const char *source);

#include <stdbool.h>

// Scan the next token from the source
Token scanToken();

#endif
