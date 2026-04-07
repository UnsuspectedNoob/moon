// scanner.c

#include <stdio.h>
#include <string.h>

#include "scanner.h"
#include <stdbool.h>

Scanner scanner;

void initScanner(const char *source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
  scanner.column = 1;
  scanner.interpolationDepth = 0;
}

// -- Helper Functions --

static bool isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isDigit(char c) { return c >= '0' && c <= '9'; }

static bool isAtEnd() { return *scanner.current == '\0'; }

static char advance() {
  scanner.current++;
  scanner.column++;
  return scanner.current[-1];
}

static char peek() { return *scanner.current; }

static char peekNext() {
  if (isAtEnd())
    return '\0';
  return scanner.current[1];
}

static bool match(char expected) {
  if (isAtEnd())
    return false;

  if (*scanner.current != expected)
    return false;

  scanner.current++;
  return true;
}

static Token makeToken(TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;
  token.column = scanner.column - token.length;
  return token;
}

static Token errorToken(const char *message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;
  token.column = scanner.column - token.length;
  return token;
}

static void skipWhitespace() {
  for (;;) {
    char c = peek();
    switch (c) {
    case ' ':
    case '\r':
    case '\t':
      advance();
      break;

    // Handle Comments (##)
    case '#':
      while (peek() != '\n' && !isAtEnd())
        advance();
      break;
    default:
      return;
    }
  }
}

// -- Literal Handlers --

static TokenType checkKeyword(int start, int length, const char *rest,
                              TokenType type) {
  if (scanner.current - scanner.start == start + length &&
      memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
  // Trie-like switch for Keyword matching
  switch (scanner.start[0]) {
  case 'a': {
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'd':
        return checkKeyword(2, 1, "d", TOKEN_ADD);
      case 'n':
        return checkKeyword(2, 1, "d", TOKEN_AND);
      case 's':
        // --- THE NEW SPLIT ---
        if (scanner.current - scanner.start == 2)
          return TOKEN_AS;
      }
    }
    break;
  }

  case 'b': {
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'e':
        return checkKeyword(2, 0, "", TOKEN_BE); // be
      case 'r':
        return checkKeyword(2, 3, "eak", TOKEN_BREAK); // break
      case 'y':
        return checkKeyword(2, 0, "", TOKEN_BY);
      }
    }
    break;
  }

  case 'e': {
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'a':
        return checkKeyword(2, 2, "ch", TOKEN_EACH);
      case 'l':
        return checkKeyword(2, 2, "se", TOKEN_ELSE); // else
      case 'n':
        return checkKeyword(2, 1, "d", TOKEN_END); // end
      }
    }
    break;
  }

  case 'f': {
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'a':
        return checkKeyword(2, 3, "lse", TOKEN_FALSE); // false
      case 'o':
        return checkKeyword(2, 1, "r", TOKEN_FOR); // for
      case 'r':
        return checkKeyword(2, 2, "om", TOKEN_FROM); // from
      }
    }
    break;
  }

  case 'g':
    return checkKeyword(1, 3, "ive", TOKEN_GIVE);

  case 'i': {
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'f':
        return checkKeyword(2, 0, "", TOKEN_IF); // if
      case 'n':
        return checkKeyword(2, 0, "", TOKEN_IN); // in
      case 's':
        return checkKeyword(2, 0, "", TOKEN_IS); // is
      }
    }
    break;
  }

  case 'k': {
    return checkKeyword(1, 3, "eep", TOKEN_KEEP);
  }

  case 'l': {
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'e':
        return checkKeyword(2, 1, "t", TOKEN_LET);
      case 'o':
        return checkKeyword(2, 2, "ad", TOKEN_LOAD);
      }
    }
    break;
  }

  case 'm': {
    return checkKeyword(1, 2, "od", TOKEN_MOD);
  }

  case 'n': {
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'o':
        return checkKeyword(2, 1, "t", TOKEN_NOT);
      case 'i':
        return checkKeyword(2, 1, "l", TOKEN_NIL);
      }
    }

    break;
  }

  case 'o': {
    return checkKeyword(1, 1, "r", TOKEN_OR);
  }

  case 'q': {
    return checkKeyword(1, 3, "uit", TOKEN_QUIT);
  }

  case 'r': {
    return checkKeyword(1, 5, "eturn", TOKEN_GIVE);
  }

  case 's': {
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'e':
        return checkKeyword(2, 1, "t", TOKEN_SET); // set
      case 'k':
        return checkKeyword(2, 2, "ip", TOKEN_SKIP); // skip
      }
    }
    break;
  }

  case 't': {
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'h':
        return checkKeyword(2, 2, "en", TOKEN_THEN); // then
      case 'o':
        return checkKeyword(2, 0, "", TOKEN_TO); // to
      case 'r':
        return checkKeyword(2, 2, "ue", TOKEN_TRUE); // true
      case 'y':
        return checkKeyword(2, 2, "pe", TOKEN_TYPE); // type
      }
    }
    break;
  }

  case 'u': {
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'n': {
        if (scanner.start[2] == 'l')
          return checkKeyword(3, 3, "ess", TOKEN_UNLESS); // unless
        if (scanner.start[2] == 't')
          return checkKeyword(3, 2, "il", TOKEN_UNTIL); // until
        break;
      }

      case 'p':
        return checkKeyword(2, 4, "date", TOKEN_UPDATE);
      }
    }

    break;
  }

  case 'w': {
    if (scanner.current - scanner.start > 1) {
      switch (scanner.start[1]) {
      case 'h':
        return checkKeyword(2, 3, "ile", TOKEN_WHILE);
      case 'i':
        return checkKeyword(2, 2, "th", TOKEN_WITH); // <--- NEW
      }
    }
    break;
  }
  }

  return TOKEN_IDENTIFIER;
}

static Token identifier() {
  while (isAlpha(peek()) || isDigit(peek()))
    advance();
  return makeToken(identifierType());
}

static Token number() {
  while (isDigit(peek()))
    advance();

  // Look for a fractional part.
  if (peek() == '.' && isDigit(peekNext())) {
    // Consume the ".".
    advance();

    while (isDigit(peek()))
      advance();
  }

  return makeToken(TOKEN_NUMBER);
}

static Token string(bool isResuming) {
  for (;;) {
    char c = peek();

    if (isAtEnd())
      return errorToken("Unterminated string.");

    if (c == '"') {
      advance(); // Consume the quote

      // If we are resuming an outer string, this quote finishes the entire
      // interpolation!
      if (isResuming) {
        scanner.interpolationDepth--;
        return makeToken(TOKEN_STRING_CLOSE);
      }

      // Otherwise, it's just a normal string ending (like "not").
      return makeToken(TOKEN_STRING);
    }

    if (c == '`') {
      // Check for escape (double backtick)
      if (peekNext() == '`') {
        advance();
        advance();
        continue;
      }

      advance(); // Consume the single backtick

      // If we are resuming an outer string, this backtick just opens another
      // hole!
      if (isResuming) {
        return makeToken(TOKEN_STRING_MIDDLE);
      }

      // Otherwise, we are opening the very first hole from a fresh string
      scanner.interpolationDepth++;
      return makeToken(TOKEN_STRING_OPEN);
    }

    if (c == '\n') {
      scanner.line++;
      scanner.column = 1;
    }
    advance();
  }
}

// -- Main Scan Function --

Token scanToken() {
  skipWhitespace();
  scanner.start = scanner.current;

  if (isAtEnd())
    return makeToken(TOKEN_EOF);

  char c = advance();

  if (isAlpha(c))
    return identifier();
  if (isDigit(c))
    return number();

  if (c == '\n') {
    scanner.line++;
    scanner.column = 1;
    return makeToken(TOKEN_NEWLINE);
  }

  switch (c) {
  case '(':
    return makeToken(TOKEN_LEFT_PAREN);
  case ')':
    return makeToken(TOKEN_RIGHT_PAREN);
  case '{':
    return makeToken(TOKEN_LEFT_BRACE);
  case '}':
    return makeToken(TOKEN_RIGHT_BRACE);
  case '[':
    return makeToken(TOKEN_LEFT_BRACKET);
  case ']':
    return makeToken(TOKEN_RIGHT_BRACKET);
  case ',':
    return makeToken(TOKEN_COMMA);
  case ':':
    return makeToken(TOKEN_COLON);
  case '.':
    return makeToken(TOKEN_DOT);
  case '-':
    return makeToken(TOKEN_MINUS);
  case '+':
    return makeToken(TOKEN_PLUS);
  case '/':
    return makeToken(TOKEN_SLASH);
  case '*':
    return makeToken(TOKEN_STAR);
  case '!':
    return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
  case '=':
    return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
  case '<':
    return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
  case '>':
    return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
  case '"':
    return string(false);
  case '`':
    if (scanner.interpolationDepth > 0) {
      return string(true); // Resume string scanning
    }
    return errorToken("Unexpected backtick.");

  case '\'':
    if (match('s'))
      return makeToken(TOKEN_POSSESSIVE);
    return errorToken("Unexpected character (orphan single quote).");
  }

  return errorToken("Unexpected character.");
}

// For testing purposes, we just print the token type
void printToken(Token token) {
  printf("%2d ", token.line);

  // Print the token name based on enum
  // (In a real app we'd have a string array for this)
  if (token.type < TOKEN_IDENTIFIER)
    printf("PUNCT  ");
  else if (token.type == TOKEN_IDENTIFIER)
    printf("IDENT  ");
  else if (token.type == TOKEN_STRING)
    printf("STRING ");
  else if (token.type == TOKEN_NUMBER)
    printf("NUMBER ");
  else if (token.type >= TOKEN_ADD)
    printf("KEYWORD");
  else
    printf("UNKNOWN");

  printf(" | '%.*s'\n", token.length, token.start);
}
