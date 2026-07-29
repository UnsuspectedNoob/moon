# Moon Compiler: Final Year Defense Documentation

This document details the internal architecture, memory models, parsing mechanisms, and execution pipeline of the Moon programming language.

---

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


---

# Documentation: `value.h` & `value.c` (Core Value System)

## Overview
- **Purpose**: This module defines how the Moon programming language represents all its core data types (Primitives and Pointers) at the lowest memory level. To maximize performance and memory efficiency, Moon uses a technique called "NaN Boxing". This technique packs a floating-point number, boolean, `nil`, or an object pointer perfectly inside a single 64-bit `uint64_t` value!

## Structs

### `ValueArray`
- **Fields**: `int capacity, int count, Value *values`
- **Description**: A dynamic array (vector) for storing `Value`s. It automatically resizes its `values` array by doubling its capacity whenever it runs out of space. This is used extensively by the compiler to hold constants and by list objects to hold elements.

## Data Types & NaN Boxing

### `typedef uint64_t Value;`
- **Description**: The absolute core of Moon's memory model. Everything in Moon is represented as a 64-bit `Value`. 

### The Masks (`SIGN_BIT`, `QNAN`, `TAG_*`)
- **Description**: In the IEEE 754 specification for 64-bit doubles, there is a massive range of bits reserved for "Not a Number" (NaN). Moon "steals" these unused bits! 
  - If a 64-bit value is a valid double, it's just a regular Number.
  - If the `QNAN` bits are set, it's a special Moon value.
  - The lowest bits of the mantissa (`TAG_NIL`, `TAG_FALSE`, `TAG_TRUE`) indicate exactly which primitive it is.
  - If the `SIGN_BIT` is also set, it means the lowest 48 bits actually hold a memory address pointing to a heap-allocated `Obj` (like a String or List).

## Functions

### `static inline double valueToNum(Value value)` & `static inline Value numToValue(double num)`
- **Description**: Uses a C `union` for "Type Punning" to safely trick the C compiler into converting between a raw 64-bit unsigned integer and a 64-bit floating point double without accidentally corrupting the bits or invoking undefined behavior.

### `void initValueArray(ValueArray *array)`
- **Description**: Initializes an empty dynamic array, setting its capacity and count to 0.

### `void writeValueArray(ValueArray *array, Value value)`
- **Description**: Appends a `Value` to the dynamic array. If the array has run out of space, it allocates a larger chunk of memory and moves the existing items over.

### `void freeValueArray(ValueArray *array)`
- **Description**: Frees the memory block managed by the array.

### `uint32_t hashValue(Value value)`
- **Description**: Generates a robust 32-bit hash for any `Value`. For heap-allocated Strings, it accesses their FNV-1a cached hash. For primitives (like numbers and booleans), it employs a fast, bit-shifting integer hash function.

### `void printValue(Value value)`
- **Description**: Prints the human-readable representation of the `Value` to standard output. If the value is a heap object, it delegates the formatting to `printObject`.


---

# Documentation: `object.h` & `object.c` (Heap Allocation Module)

## Overview
- **Purpose**: While the `value.h` module handles primitives that fit inside a 64-bit value, the `object.h` module handles dynamically allocated, complex data structures that live on the Heap (like Strings, Lists, Dictionaries, and Functions). Every heap-allocated structure in Moon "inherits" from a base `Obj` struct. This allows the Garbage Collector (GC) to treat all complex objects uniformly when tracing and sweeping memory.

## Structs (The Object Hierarchy)

### `Obj` (The Base Class)
- **Fields**: `ObjKind type`, `bool isMarked`, `struct Obj *next`
- **Description**: The fundamental header struct that sits at the very beginning of *every* heap object in memory. 
  - `type` identifies what kind of object it is (String, List, etc.).
  - `isMarked` is used by the mark-and-sweep Garbage Collector to track if the object is still reachable.
  - `next` is an intrusive linked-list pointer. Every allocated object in Moon is strung together into one massive linked list managed by the VM!

### `ObjString`
- **Fields**: `Obj obj`, `int length`, `char *chars`, `uint32_t hash`, `struct ObjString *left`, `struct ObjString *right`
- **Description**: Represents a string of text. To achieve incredible performance during string concatenation, Moon strings can act as a **Rope Data Structure**. If a string is created by joining two others together, it simply points to them (`left` and `right`) without allocating new memory for the characters! It is only "flattened" into a contiguous array of characters (`chars`) when strictly necessary. Strings are also heavily interned in a global Hash Table to save memory and allow strings to be compared for equality by simply comparing their memory addresses!

### `ObjFunction`
- **Fields**: `Obj obj`, `int arity`, `Chunk chunk`, `ObjString *name`, `ObjModule *module`, `Table *homeGlobals`, `bool isTopLevel`
- **Description**: The compiled representation of a user-defined function or a script. It holds the actual bytecode `chunk` that the Virtual Machine will execute, alongside metadata about how many arguments it expects (`arity`). It also keeps a pointer back to its `module` and `homeGlobals` to ensure it can always access its original environment variables, no matter where it is executed!

### `ObjMultiFunction`
- **Fields**: `Obj obj`, `ObjString *name`, `int arity`, `int methodCount`, `int methodCapacity`, `Value *methods`, `Value **signatures`
- **Description**: The engine behind Moon's powerful **Multiple Dispatch** feature. When you declare multiple versions of the same Phrasal Function (e.g. `add (x: Number)` and `add (x: String)`), Moon wraps them all up in an `ObjMultiFunction`. When the phrase is called, the VM intercepts this object, looks at the runtime types of the arguments provided, cross-references them against the `signatures` table, and executes the correct corresponding bytecode chunk in the `methods` array!

### `ObjList`
- **Fields**: `Obj obj`, `int count`, `int capacity`, `Value *items`
- **Description**: A dynamically growing contiguous array (vector) for ordered collections. It works identically to `ValueArray` but is explicitly allocated on the heap as an object so it can be passed by reference and managed by the GC.

### `ObjDict`
- **Fields**: `Obj obj`, `Table fields`
- **Description**: A collection of key-value pairs. It directly wraps the highly optimized `Table` struct (used elsewhere in the compiler for variables) to provide instant Hash Map lookups.

### `ObjType` & `ObjInstance`
- **Fields**: `Obj obj`, `ObjString *name`, `Table properties`, `bool isNative` (ObjType)
- **Fields**: `Obj obj`, `ObjType *type`, `Table fields` (ObjInstance)
- **Description**: Moon's Custom Data Blueprint system. 
  - `ObjType` acts as the template or blueprint. It holds the name and any static properties or methods.
  - `ObjInstance` is the actual physical clone living in memory. It holds its own unique data in its `fields` dictionary, and keeps an invisible pointer back to its parent `ObjType` to figure out what methods it can run!

### `ObjUnion`
- **Fields**: `Obj obj`, `int count`, `Value *types`
- **Description**: Represents a type signature that accepts multiple different blueprints (e.g., `Number or String`). When generating signatures for `ObjMultiFunction`s, unions are evaluated to ensure all possible paths are covered.

### `ObjModule`
- **Fields**: `Obj obj`, `ObjString *name`, `ObjString *source`, `Table fields`
- **Description**: Represents an imported file or distinct execution space. It encapsulates its own global variables inside the `fields` table to prevent variable name collisions between different files!

## Core Functions

### `uint32_t hashString(const char *key, int length)`
- **Description**: Implements the FNV-1a Hash Algorithm. It generates an extremely uniform 32-bit hash for a given string of text, which powers all dictionary lookups and variable resolutions in Moon.

### `void flattenString(ObjString *string)`
- **Description**: The core algorithm for Moon's String Ropes. It performs an iterative Depth-First Search (DFS) over the binary tree of nested strings, allocating a single contiguous block of memory, copying all leaf characters into it, and completely severing the tree branches to leave a perfectly flat, cache-friendly string.

### `void stringifyValueToBuffer(Value value, int indent, StringBuffer* sb)`
- **Description**: The engine behind the `show` command. It takes any Moon value, inspects its type, and meticulously formats it into a human-readable text string. For complex collections like `ObjDict`, it recursively calls itself to seamlessly pretty-print deeply nested structures!

### Object Allocators (`newFunction`, `newList`, `newDict`, `newType`, `newInstance`, etc.)
- **Description**: A suite of constructor functions. Every single one of these internally calls `allocateObject()`, which asks the GC for memory, stamps the base `Obj` header, and registers the new object with the VM's linked list to guarantee it will eventually be swept away when no longer needed.


---

# Documentation: `ast.h` & `ast.c` (Abstract Syntax Tree)

## Overview
- **Purpose**: The AST module defines the hierarchical tree structure that represents the syntactic structure of Moon source code. After the Scanner breaks the source code into Tokens, the Parser reads those Tokens and builds an Abstract Syntax Tree (AST). This tree is purely structural; it strips away formatting and semicolons, leaving only the logical relationships between operations and their operands. Moon uses a massive, unified `Node` struct with a highly optimized memory release mechanism.

## Structs (The Node Hierarchy)

### `NodeType` (enum)
- **Description**: An enumeration defining every possible type of node in the language. Examples include `NODE_LITERAL` (for numbers and strings), `NODE_BINARY` (for math and logic operations), `NODE_IF` (for conditionals), and `NODE_PHRASAL_CALL` (for invoking Moon's unique phrasal functions).

### `struct sNode` (The Master Node)
- **Fields**: `NodeType type`, `Node *parent`, `int line`, `bool usesIt`, `union as`
- **Description**: Moon does not use traditional C inheritance (like casting base structs to child structs). Instead, it uses a **Tagged Union**. Every single node in the tree is represented by this exact same struct. 
  - `type` identifies what kind of node it is.
  - `parent` is an invisible pointer pointing back up the tree, which is crucial for advanced traversal or error tracing.
  - `usesIt` is a powerful boolean flag used for implicit variable injection. If this node (or any of its children) uses the magic `it` keyword, this flag propagates all the way up the tree to automatically convert blocks into lambdas!
  - `union as` is the payload. It perfectly overlaps the memory of every possible payload struct (like `BinaryPayload` or `IfPayload`), ensuring a `Node` always takes exactly the same amount of memory regardless of what it represents, preventing heap fragmentation.

### Payload Structs (The Union Members)
- **Description**: These structs define the specific data each node needs to function.
  - **`BinaryPayload`**: Holds `left` (Node), `opToken` (Token), and `right` (Node).
  - **`PhrasalCallPayload`**: The complex engine behind Phrasal Functions. It holds the `mangledName` (e.g. `add$1_to$1`), the `arguments` array, and the `phraseTokens` array used to preserve the original English keywords for error reporting.
  - **`InterpolationPayload`**: Represents an interpolated string. It holds an array of `parts` which alternate between string literals and executable expression nodes.
  - **`ComprehensionPayload`**: Represents list/dict comprehensions (e.g., `[x * 2 for x in list]`). It tracks the `iterator`, `indexVar`, the `sequence` being iterated over, and the `body` expression.

## Functions

### `Node *allocateNode(NodeType type, int line)`
- **Description**: The internal base constructor. It allocates a single block of memory for a Node, zero-initializes it, sets the `type` and `line` number, and defaults the `parent` pointer to `NULL`.

### Node Constructors (`newBinaryNode`, `newIfNode`, `newPhrasalCallNode`, etc.)
- **Description**: A vast suite of constructor functions used by the Parser. Each constructor is responsible for allocating the base node, filling in its specific payload, and crucially, propagating the `usesIt` flag upwards. If any child node passed into the constructor has `usesIt == true`, the constructor will automatically set `usesIt = true` on the newly created parent node! It also automatically links the `parent` pointer of the children back to the new node.

### `void freeNode(Node *root)`
- **Description**: The AST Destructor. Since the AST can grow incredibly deep (e.g., a massive math equation or deeply nested blocks), recursively freeing it using C functions would cause a **Stack Overflow**. To prevent this, `freeNode` implements a brilliant **Iterative Depth-First Search (DFS)**. It creates a custom, heap-allocated stack (`NodeArray`), pushes the root node onto it, and processes the tree iteratively. For every node it pops off, it manually frees any dynamically allocated payload arrays (like a list of function arguments), pushes its children onto the stack, and finally frees the base node struct. This completely defuses the call stack bomb!


---

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


---

# Documentation: Compilation Pipeline (`compiler`, `codegen`, & `emitter`)

## Overview
- **Purpose**: This trio of modules represents the "Backend" of the Moon compiler. Together, they take the Abstract Syntax Tree (AST) generated by the parser and lower it into flat, high-performance Bytecode. 
  - `compiler.c` acts as the master orchestrator.
  - `codegen.c` recursively walks the AST nodes.
  - `emitter.c` manages local variable scopes and writes the actual raw bytes into the function's `Chunk`.

---

## 1. `compiler.h` & `compiler.c` (The Orchestrator)

### `ObjFunction *compile(const char *source, ObjModule *module, int startLine)`
- **Description**: The master entry point for executing Moon code. It receives raw text `source`, passes it to `parseSource()` to generate the AST, and then immediately hands that AST to `generateCode()`. If any syntax or compilation errors occur, it gracefully returns `NULL`. Otherwise, it returns a fully baked `ObjFunction` ready for the Virtual Machine!

---

## 2. `codegen.h` & `codegen.c` (The AST Walker)

### Struct: `CompilerLoop`
- **Fields**: `int start`, `int scopeDepth`, `int breakJumps[256]`, `int breakCount`, `struct sCompilerLoop *enclosing`
- **Description**: Tracks the state of currently active loops (like `while` and `for`). 
  - `start` marks the byte offset to jump back to when the loop repeats.
  - `breakJumps` tracks the exact locations of every `break` statement inside the loop so they can be "patched" with the correct jump-out offset once the loop's end is finally reached.
  - `enclosing` allows nested loops to function perfectly.

### `ObjFunction *generateCode(Node *rootAST)`
- **Description**: The entry point for bytecode generation. It initializes the root `Compiler` state (acting as the implicit "script" function) and begins recursively walking the AST tree starting from `rootAST`.

### `static void walkNode(Node *node)`
- **Description**: A massive `switch` statement that routes every `NodeType` to its specific bytecode generation logic. It translates abstract operations (like `NODE_BINARY`) into concrete CPU-like instructions (like `OP_ADD`).

### `static void startLoop(CompilerLoop *loop, int startByte)` & `static void endLoop()`
- **Description**: Lifecycle hooks for `while` and `for` loops. `startLoop` registers the new loop state, and `endLoop` resolves all pending `break` jumps and gracefully pops the loop off the tracking stack.

### `static uint16_t identifierConstant(Token *name)`
- **Description**: Takes a variable or function name, interns it into a String Object, and stores it in the Chunk's constant table, returning the index where it was stored.

---

## 3. `emitter.h` & `emitter.c` (The Bytecode Writer)

### Struct: `Compiler`
- **Fields**: `Compiler *enclosing`, `ObjFunction *function`, `FunctionType type`, `Chunk *chunk`, `Local locals[UINT8_COUNT]`, `int localCount`, `int scopeDepth`, `int temporaries`
- **Description**: The core state of the bytecode emitter. Since functions can be nested inside other functions in Moon, each function gets its own `Compiler` struct. 
  - `locals` is a fixed-size array tracking every local variable currently in scope.
  - `scopeDepth` tracks how deeply nested we are in curly braces `{}` or control flow blocks.
  - `temporaries` tracks invisible variables generated by the compiler (e.g., for loop iterators).

### Struct: `Local`
- **Fields**: `Token name`, `int depth`, `bool isCaptured`, `int slot`
- **Description**: Represents a single local variable. 
  - `depth` records the block scope where it was declared.
  - `isCaptured` will be used for Closures.
  - `slot` is the exact physical index on the Virtual Machine's call stack where this variable will live at runtime!

### Scope Management (`beginScope`, `endScope`)
- **Description**: `beginScope()` increments `scopeDepth`. `endScope()` decrements it, and crucially, walks backward through the `locals` array, emitting `OP_POP` instructions to instantly destroy any local variables that just went out of bounds!

### Local Variables (`addLocal`, `resolveLocal`, `declareVariable`, `defineVariable`)
- **Description**: Handles the lifecycle of variables. 
  - `declareVariable` registers a new local into the `locals` array but leaves it uninitialized. 
  - `resolveLocal` searches the `locals` array backward to find the most recently declared variable with a matching name. If found, it operates on the stack slot. If not, it assumes the variable is Global!

### `Chunk *currentChunk()`
- **Description**: A simple helper that returns the `Chunk` of the function currently being compiled (the one at the top of the `Compiler` stack).

### Bytecode Emission (`emitByte`, `emitBytes`, `emitReturn`)
- **Description**: The lowest-level functions in the backend. They append raw 8-bit `uint8_t` opcodes and operands directly into the `Chunk`'s dynamically growing memory array.

### Constant Management (`makeConstant`, `emitConstant`)
- **Description**: Pushes a `Value` (like a huge number or string) into the Chunk's constant table and emits an `OP_CONSTANT` instruction with the resulting index. If the index exceeds 255 (requiring more than 1 byte to store), it dynamically upgrades to an `OP_CONSTANT_16` instruction!

### Jump Management (`emitJump`, `patchJump`, `emitLoop`)
- **Description**: The engine behind `if` statements and loops. 
  - `emitJump` writes a jump instruction with a dummy offset (like `0xFFFF`) and returns its location.
  - `patchJump` is called later, once the destination is known. It calculates the exact distance to jump and goes back to overwrite the dummy offset with the correct jump distance!
  - `emitLoop` writes a backwards jump (`OP_LOOP`) to repeat a block of code.


---

# Documentation: Execution Pipeline (`vm`, `chunk`, & `memory`)

## Overview
- **Purpose**: This trio of modules forms the Runtime Engine of the Moon compiler. After the compiler has lowered the AST into bytecode, the Virtual Machine (VM) takes over to execute those bytes at blazing fast speeds.
  - `chunk.h/c` defines the bytecode instruction set and the data structures that hold the compiled code.
  - `memory.h/c` manages all dynamic memory allocation and contains the Mark-and-Sweep Garbage Collector (GC).
  - `vm.h/c` is the core execution loop, managing the call stack, global variables, and interpreting opcodes.

---

## 1. `chunk.h` & `chunk.c` (The Instruction Set Architecture)

### Struct: `Chunk`
- **Fields**: `int count`, `int capacity`, `uint8_t *code`, `int *lines`, `ValueArray constants`, `InlineCacheArray caches`
- **Description**: A `Chunk` is a sequence of bytecode. 
  - `code` is a dynamically growing array of raw bytes containing the actual executable instructions (`OpCode`s).
  - `lines` runs parallel to `code` and tracks the original source code line number for every byte to provide accurate stack traces.
  - `constants` is a pool of `Value`s used in the code.
  - `caches` is an optimization structure used to speed up Phrasal Function multiple-dispatch lookups.

### Struct: `InlineCacheEntry` & `InlineCacheArray`
- **Description**: These structures support Moon's multiple dispatch system. When a phrasal function is called, the VM caches the exact types of the arguments passed. The next time the function is called, if the argument types match the `InlineCacheEntry`, the VM completely skips the expensive signature lookup!

### Functions
#### `void initChunk(Chunk *chunk)` & `void freeChunk(Chunk *chunk)`
- **Description**: Lifecycle functions. `initChunk` zeroes out all fields, and `freeChunk` uses the memory module's `FREE_ARRAY` macros to deallocate the arrays.

#### `void writeChunk(Chunk *chunk, uint8_t byte, int line)`
- **Description**: Appends a byte of bytecode (an OpCode or operand) to the chunk's `code` array, dynamically growing it if capacity is reached. Also records the line number.

#### `int addConstant(Chunk *chunk, Value value)`
- **Description**: Appends a constant to the `constants` ValueArray and returns its index. 

#### `int addCacheEntry(Chunk *chunk)`
- **Description**: Appends an empty cache slot to the `caches` array and returns its index, to be populated later by the VM during runtime execution.

---

## 2. `vm.h` & `vm.c` (The Virtual Machine)

### Struct: `CallFrame`
- **Fields**: `ObjFunction *function`, `uint8_t *ip`, `Value *slots`, `Table *globals`, `Value stickySubject`
- **Description**: Represents a single active function call. 
  - `ip` (Instruction Pointer) points directly into the function's bytecode chunk.
  - `slots` points to the section of the VM's massive data stack reserved for this function's local variables.
  - `stickySubject` holds the state for Moon's unique chained comparisons (e.g., `if x == 5 or == 10`).

### Struct: `VM`
- **Fields**: `CallFrame frames[FRAMES_MAX]`, `Value stack[STACK_MAX]`, `Value *stackTop`, `Obj *objects`, `size_t bytesAllocated`
- **Description**: The global singleton state. It holds the Call Stack (`frames`) and the Data Stack (`stack`). It also maintains pointers to all heap-allocated objects (`objects`) so the GC can trace them, and tracks `bytesAllocated` to trigger GC sweeps.

### Global Functions
#### `void initVM()` & `void freeVM()`
- **Description**: `initVM` zeroes the stack pointers, initializes global hash tables (like the string intern pool), and prepares the GC state. `freeVM` calls `freeObjects()` to destroy the entire heap.

#### `void bootstrapCore()`
- **Description**: An essential initialization step that injects all the native C functions (like standard math and string libraries) into the global scope before user code runs.

#### `InterpretResult interpret(const char *source, int startLine)`
- **Description**: The highest-level entry point. It calls `compile()`, loads the resulting script function into a `CallFrame`, and triggers `run()`.

#### `static InterpretResult run()`
- **Description**: The hottest loop in the compiler. It does NOT use a standard C `switch` statement! Instead, it uses **Computed Gotos** (`goto *dispatchTable[instruction]`) combined with a `DISPATCH()` macro. This advanced optimization skips the overhead of a switch loop entirely, jumping directly to the C logic for each opcode (like `TARGET_OP_ADD`), executing it, and jumping straight to the next!

#### Stack Operations: `void push(Value value)`, `Value pop()`, `Value peek(int distance)`
- **Description**: Highly optimized macros/functions for interacting with the VM's data stack. 

#### `bool isFalsey(Value value)` & `bool valuesEqual(Value a, Value b)`
- **Description**: Centralized logic for type comparison. `isFalsey` dictates that only `nil` and `false` are considered false.

#### Static Linker API: `void registerNativePhrasal(...)`, `void defineNative(...)`
- **Description**: Exposes C functions to Moon by wrapping them in `ObjNative` or `ObjMultiFunction` structs and injecting them into the global variables table.

#### Error Handling: `void runtimeErrorDetailed(...)`, `void throwNativeError(...)`
- **Description**: Halts execution safely. It walks backward down the `frames` stack to print a beautiful, multi-layered stack trace, utilizing the `lines` array stored in the Chunks!

---

## 3. `memory.h` & `memory.c` (The Garbage Collector)

### Macros (`ALLOCATE`, `FREE`, `GROW_ARRAY`)
- **Description**: Wrappers around `reallocate()`. By routing all allocations through a single point, the VM accurately tracks `bytesAllocated`.

### Memory Functions
#### `void *reallocate(void *pointer, size_t oldSize, size_t newSize)`
- **Description**: The memory controller. If `newSize` is larger than `oldSize`, it allocates more memory. Crucially, if total `bytesAllocated` exceeds the `nextGC` threshold, this function immediately suspends execution and triggers `collectGarbage()` before allocating!

#### `void freeObject(Obj *object)` & `void freeObjects()`
- **Description**: Handles the teardown of specific objects based on their `ObjType` (e.g. freeing the internal character array of a string). `freeObjects` sweeps the entire heap on VM shutdown.

### Garbage Collection API
#### `void collectGarbage()`
- **Description**: The master trigger for the GC cycle. It calls `markRoots()`, then `traceReferences()`, and finally `sweep()`.

#### `static void markRoots()`
- **Description**: Sweeps through all intrinsically accessible values: variables on the data stack, global variables, and constants in active CallFrames, passing them to `markValue()`.

#### `void markValue(Value value)` & `void markObject(Obj *object)`
- **Description**: Checks if the value is a heap object. If it is, and its `isMarked` flag is false, it flips it to true. Then, it pushes the object onto the VM's `grayStack` to be traced later.

#### `static void traceReferences()` & `static void blackenObject(Obj *object)`
- **Description**: The iterative tracer. It pops objects off the `grayStack` and "blackens" them by calling `markValue` on any internal references they hold (e.g., if it's a List, it marks all values inside the list). Using the `grayStack` completely eliminates recursive C stack overflows!

#### `static void sweep()`
- **Description**: Walks the VM's massive linked list of *all* allocated objects (`vm.objects`). If an object is NOT marked (meaning `traceReferences` never reached it), it is completely inaccessible and immediately passed to `freeObject()`! It then resets `isMarked` for the survivors.


---

# Documentation: Advanced Features (`sigtrie` & `subscript`)

## Overview
- **Purpose**: This module covers two of the most complex runtime features in the Moon language: 
  - `sigtrie.h/c` implements the **Signature Trie**, a sophisticated data structure used to rapidly map Moon's natural-language "Phrasal Functions" (e.g., `add (x) to (y)`) into their internal mangled names (`add$1_to$1`).
  - `subscript.h/c` provides the heavy-lifting logic for dynamic indexing and slicing of Lists, Dictionaries, and Strings (e.g., `list[1 to 5 by 2]`).

---

## 1. `sigtrie.h` & `sigtrie.c` (The Signature Trie)

### Struct: `RegistryEntry`
- **Fields**: `uint32_t rootHash`, `char *rootWord`, `TrieNode *trieRoot`
- **Description**: Represents a bucket in the global hash tables (`phrasalTable` and `propertyTable`). The `rootHash` is cached to speed up string comparisons during the linear probing sequence. 

### Struct: `TrieNode`
- **Fields**: `PhraseNodeType type`, `uint32_t labelHash`, `char *labelName`, `int arity`, `bool isTerminal`, `bool isCore`, `char *mangledName`, `TrieNode **children`, `int childCount`, `int childCapacity`
- **Description**: A single node in the Trie. 
  - `type` dictates if the node is a `NODE_LABEL` (a specific keyword like "to") or a `NODE_ARGUMENT` (a variable slot).
  - `isTerminal` flags if this node marks the end of a valid phrase. If true, `mangledName` holds the compiled C-string name used by the VM.
  - `isCore` flags if this node was created during `bootstrapCore()`. This allows user-defined phrases to be wiped between REPL inputs without destroying the standard library!

### Hash Tables
#### `static RegistryEntry phrasalTable[TABLE_CAPACITY];` & `static RegistryEntry propertyTable[TABLE_CAPACITY];`
- **Description**: Two massive, statically allocated Hash Tables (capacity 1024) that use **Open Addressing with Linear Probing** to resolve collisions. They store the "Root Word" of every phrase (e.g., the `add` in `add (x) to (y)`).

### Core Functions
#### `TrieNode *getSignatureTrie(const char *rootWord)`
- **Description**: Executes the O(1) hash lookup with linear probing to find the root `TrieNode` for a given starting word.

#### `TrieNode *startPhrase(const char *rootWord, int length)`
- **Description**: Searches the hash table for the root word. If it doesn't exist, it allocates a new `RegistryEntry`, initializes a new `NODE_LABEL` TrieNode, and inserts it into the table.

#### `TrieNode *addLabelBranch(...)` & `TrieNode *addArgumentBranch(...)`
- **Description**: Tree builders. They linearly scan a node's `children` array to see if the branch (e.g., the keyword "to" or an argument of arity `1`) already exists. If not, they dynamically allocate a new `TrieNode` and append it to the dynamically resizing `children` array.

#### `void registerSignature(const char *root, const char *path, const char *mangledName)`
- **Description**: The master compiler function used during initialization. It takes a raw path string (like `$,to,$`), tokenizes it using C's `strtok()`, and iteratively walks down the Trie, calling `addArgumentBranch()` for `$` tokens and `addLabelBranch()` for text tokens, finally calling `finalizePhrase()` on the leaf node.

#### `void freeSignatureTable()` & `static void freeTrieNode(TrieNode *node)`
- **Description**: A highly intelligent destructor. When called, it recursively walks the Trie and frees all memory. Crucially, it checks the `isCore` flag. It will completely bypass and preserve any branches belonging to the core standard library while mercilessly destroying user-defined branches!

---

## 2. `subscript.h` & `subscript.c` (Dynamic Indexing & Slicing)

### Core Functions
#### `bool executeGetSubscript(Value seqVal, Value indexVal, Value *result)`
- **Description**: The massive routing function called by the `OP_GET_SUBSCRIPT` bytecode instruction. It inspects the `seqVal` type at runtime and routes it to the correct extraction logic:
  - **Lists**: Allows both single Number access and Range slicing. It enforces **1-based indexing** (converting user index 1 to C index 0). It also fully supports **Negative Indexing** (e.g., `-1` becomes `list->count - 1`)! If a `Range` is passed (e.g. `1 to 10 by 2`), it dynamically allocates a *new* List and uses a step-aware `for` loop to slice the original array.
  - **Strings**: Identical logic to Lists, but extracts `char`s. When single-indexed, it returns a 1-character String Object. When sliced, it allocates a new C-string buffer, populates it, and hands it off to `takeString()`.
  - **Dictionaries**: Bypasses indexing entirely and directly calls `tableGet()` to hash the `indexVal` as a key. If missing, it safely returns `NIL_VAL`.

#### `bool executeSetSubscript(Value collectionVal, Value indexVal, Value value, Value *result)`
- **Description**: The routing function for `OP_SET_SUBSCRIPT`. 
  - **Dictionaries**: Calls `tableSet()` to insert or overwrite the key-value pair.
  - **Lists**: Calculates the 1-based (and potentially negative) index and directly overwrites the pointer in the `list->items` array. Note: Unlike Python, Moon does *not* support replacing slices with other slices!
  - **Strings**: Strings in Moon are immutable! If a user tries to assign to a string index, this function intentionally falls through to a `runtimeErrorDetailed()`!


---

