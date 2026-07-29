# Documentation: `parser.h` & `parser.c` (The Syntax Parser)

## Overview
- **Purpose**: The Parser is the heart of the Moon frontend. It consumes the stream of `Token`s produced by the Scanner and uses them to build the Abstract Syntax Tree (AST). Moon uses a highly customized **Pratt Parser** (Top-Down Operator Precedence). This architecture allows it to elegantly handle Moon's unique brace-free syntax, chained phrasal functions, implicit `it` lambdas, and deep mathematical expressions without becoming a tangled mess of recursive functions.

## Structs

### `Parser`
- **Fields**: `Token current`, `Token previous`, `bool hadError`, `bool panicMode`
- **Description**: The global state tracker for the parsing process. 
  - `current` is the token currently being evaluated.
  - `previous` is the token we just successfully consumed.
  - `hadError` flags if the overall compilation has failed, preventing execution.
  - `panicMode` suppresses cascading error messages during a syntax error until the parser resynchronizes.

### `ExpectedLabel`
- **Fields**: `uint32_t hash`, `int depth`
- **Description**: Enables **Phrasal Functions**. When the parser enters a phrasal function (e.g., `multiply (x) by (y)`), it pushes the expected separator keyword (`by`) onto a global stack as an `ExpectedLabel`. The `hash` allows for fast string comparison, and the `depth` tracks parenthesis nesting so variables inside brackets don't accidentally satisfy outer phrasal separators.

### `ParseRule`
- **Fields**: `PrefixFn prefix`, `InfixFn infix`, `Precedence precedence`
- **Description**: The blueprint for the Pratt Parser table. Every single token type maps to a `ParseRule`. 
  - `prefix` is called if the token starts an expression (e.g., `-` or `let`).
  - `infix` is called if the token is in the middle of an expression (e.g., `+`).
  - `precedence` dictates how tightly the operator binds (`*` before `+`).

## Global/Static Variables

### `static ExpectedLabel expectedLabelStack[256];` & `static int expectedLabelCount = 0;`
- **Description**: The stack managing the expected separators for currently executing phrasal functions. 

### `static int groupingDepth = 0;`
- **Description**: Tracks how deep the parser is inside parentheses `()`, brackets `[]`, or braces `{}`. This ensures `expectedLabelStack` checks only trigger at the correct relative depth.

### `static int loopingDepth = 0;`
- **Description**: Tracks `while` or `for` loops. If `0`, the `break` keyword throws a syntax error.

### `static int parseDepth = 0;`
- **Description**: Tracks the recursive depth of the C call stack. Used alongside `infixDepth` to prevent C-level Stack Overflows gracefully.

### `ParseRule rules[]`
- **Description**: The core Pratt Parsing lookup table that dictates the grammar.

## Core Parsing Engine

### `Node *parseSource(const char *source, int startLine)`
- **Description**: The primary entry point. Initializes the Scanner, resets state variables, and kicks off the `declaration()` parsing loop.

### `static Node *parsePrecedence(Precedence precedence)`
- **Description**: The absolute heart of the Pratt Parser. It reads the current token, finds its `PrefixFn`, and executes it. Then, it enters a `while` loop, checking if the *next* token's precedence is strictly greater than the passed `precedence`. If so, it consumes the token and calls its `InfixFn`, passing the currently built AST node as the `left` argument!

### `static Node *expression()`
- **Description**: A wrapper around `parsePrecedence(PREC_ASSIGNMENT)` that evaluates a full expression from left to right.

## Pratt Parsing Functions (Prefix & Infix Handlers)

### `static Node *number()`, `static Node *string()`, `static Node *literal()`
- **Description**: Prefix rules for basic primitives. They consume the token and immediately return a `NODE_LITERAL`.

### `static Node *variable()`
- **Description**: Prefix rule for identifiers. It checks if the identifier might actually be a Phrasal Function call (by looking up the Signature Trie). If it's just a variable, it returns a `NODE_VARIABLE`.

### `static Node *grouping()`
- **Description**: Prefix rule for `(`. It increments `groupingDepth`, recursively calls `expression()` to parse the contents, expects a `)`, decrements `groupingDepth`, and returns the inner AST node.

### `static Node *unary()`
- **Description**: Prefix rule for `-` and `not`. It parses the operator, then calls `parsePrecedence(PREC_UNARY)` to grab the right-hand operand.

### `static Node *binary(Node *left)`
- **Description**: Infix rule for `+`, `-`, `*`, `/`, etc. It takes the `left` node, determines its own precedence from the `rules` table, and calls `parsePrecedence(precedence + 1)` to grab the right-hand side, locking the two together in a `NODE_BINARY`.

### `static Node *and_(Node *left)`, `static Node *or_(Node *left)`
- **Description**: Infix rules for short-circuiting logic operators.

### `static Node *subscript(Node *left)`
- **Description**: Infix rule for `[`. Handles array/dictionary indexing (e.g., `list[0]`).

### `static Node *dot(Node *left)`
- **Description**: Infix rule for `.`. Handles property access (e.g., `player.health`).

### `static Node *list()`, `static Node *dict()`
- **Description**: Prefix rules for `[` and `{` that parse dynamic Lists and Dictionaries, including List and Dict Comprehensions (`[x * 2 for x in list]`).

### `static Node *interpolation()` & `static Node *extractInterpolationString(Token token)`
- **Description**: Prefix rules for string interpolation (`` ` ``). These functions alternate between scanning string chunks and recursively parsing the embedded expressions, flattening them into an `InterpolationPayload`.

### `static Node *implicitIt()`
- **Description**: Handles the magic `it` keyword, tagging the generated node with `usesIt = true` to force lambda compilation higher up the tree.

### `static Node *castExpression(Node *left)`
- **Description**: Infix rule for the `as` keyword. Handles runtime type-casting (e.g., `x as String`).

### `static Node *range(Node *left)`
- **Description**: Infix rule for the `to` keyword (e.g. `1 to 10 by 2`). 

### `static Node *possessive(Node *left)`
- **Description**: Infix rule for `'s`. Translates `player's health` into standard property access `player.health`.

## Statement Handlers (Control Flow & Definitions)

### `static Node *declaration()`
- **Description**: The top-level branch. Checks if the token is a `let` (variable declaration) or `type` (class declaration). If neither, falls back to `statement()`.

### `static Node *statement()`
- **Description**: Routes control flow keywords: `if`, `while`, `for`, `return`, `break`, `skip`. If none match, assumes it's an `expressionStatement()`.

### `static Node *ifStatement(bool invert)`
- **Description**: Parses `if` blocks. The `invert` flag is true if called by `unless`, automatically negating the condition!

### `static Node *whileLogic(bool invert)`
- **Description**: Parses `while` (and `until`) loops, incrementing `loopingDepth` to allow `break`.

### `static Node *forStatement()`
- **Description**: Parses `for` loops, managing the iterator variable (e.g., `for item in list:` or `for i, item in list:`).

### `static Node *block(TokenType *terminators, int count)`
- **Description**: Parses a sequence of statements until it hits a terminator (like `end`, `else`, or `EOF`). 

### `static Node *letDeclaration()`
- **Description**: Parses variable declarations and function definitions (e.g., `let x = 5` or `let add (x) to (y):`). If it detects a parameter list, it branches into parsing a full function block.

### `static Node *typeDeclaration()`
- **Description**: Parses `type Player:` blueprints, extracting property names and their default values.

### `static Node *instantiate(Node *left)` & `static Node *instantiateWith(Node *left)`
- **Description**: Infix rules for instantiating custom types (e.g. `Player { name: "Harry" }` or `Player with name "Harry"`).

## Error & State Management Functions

### `static bool isExpectedLabel()`
- **Description**: Checks the token stream against the `expectedLabelStack` at the current `groupingDepth`.

### `void errorAt()`, `void consume()`, `void synchronize()`
- **Description**: Panic-mode error recovery utilities. `synchronize()` skips tokens after an error until a valid statement boundary is found.

### `static void resetParserState()`
- **Description**: Clears `groupingDepth`, `loopingDepth`, and `parseDepth` before a fresh parse run, ensuring state doesn't leak between compilations (especially critical for Language Servers).
