# Documentation: `scanner.h` & `scanner.c` (Lexing Module)

## Overview
- **Purpose**: The Scanner (or Lexer) is the first phase of the Moon compiler. It is responsible for reading the raw C-string source code character by character and grouping them into meaningful "Tokens" (like numbers, identifiers, or keywords) that the Parser can easily understand. It operates on demand, meaning it only scans the next token when the Parser explicitly calls `scanToken()`.

## Structs

### `Token`
- **Fields**: `TokenType type`, `const char *start`, `int length`, `int line`, `int column`, `const char *errorMessage`
- **Description**: Represents a single, indivisible lexeme of the language. Instead of dynamically allocating memory for the string value of the token, it simply stores a `start` pointer into the original source code string and a `length`, keeping token generation blazing fast with zero allocations.

### `Scanner`
- **Fields**: `const char *start`, `const char *current`, `int line`, `int column`, `int interpolationDepth`, `bool preserveComments`
- **Description**: The core state object tracking the lexer's progress through the source file. `start` points to the beginning of the current lexeme being processed, and `current` points to the character currently being evaluated.

## Global/Static Variables

### `Scanner scanner;`
- **Description**: A global singleton instance of the scanner. Since Moon is single-threaded and compiles one file at a time sequentially, using a global state avoids the overhead of passing a pointer around constantly.

## Core Functions

### `void initScanner(const char *source, int startLine)`
- **Description**: Initializes the global `scanner` state, pointing it at the raw source code string and resetting line/column tracking.

### `Token scanToken()`
- **Description**: The main entry point for the Parser. It skips whitespace (and optionally comments), identifies the type of character it's currently looking at (e.g., alpha, digit, quote), and routes execution to the appropriate specific parsing function to yield the next `Token`.

## Internal Helper Functions

### `static void skipWhitespace()`
- **Description**: Safely advances the `current` pointer past spaces, tabs, carriage returns, and comments. It uses the `consumeComment` helper for multiline `#` logic.

### `static void consumeComment(bool isMultiline, int baseColumn)`
- **Description**: Specifically handles Moon's indentation-based multiline comments. It tracks horizontal column width to silently consume text without polluting the AST until the indentation returns to the baseline.

### `static TokenType identifierType()`
- **Description**: Employs a hand-rolled "Trie-like" switch statement tree to perform insanely fast, allocation-free keyword matching. It checks the first character, then branches based on length and subsequent characters to instantly resolve if an identifier is a reserved word (like `if`, `while`, or `let`).

### `static Token string(bool isResuming)`
- **Description**: Scans string literals (e.g. `"Hello"`). It includes advanced logic for Moon's string interpolation. When it encounters a backtick (`` ` ``) inside a string, it returns a special `TOKEN_STRING_OPEN` token and pauses string scanning, allowing the Parser to evaluate the embedded expression!

### `static char advance()`, `static char peek()`, `static char peekNext()`
- **Description**: Standard utility trio for lexers. `advance()` consumes and returns a character, updating column/line numbers. `peek()` looks at the current character without consuming it. `peekNext()` looks ahead by one character.

### `static Token makeToken(TokenType type)` & `static Token errorToken(const char *message)`
- **Description**: Helper functions that package the current lexer position and state into a finalized `Token` struct to hand back to the Parser.
