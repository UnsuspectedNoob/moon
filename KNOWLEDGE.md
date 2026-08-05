# MOON Language Master Knowledge Base (`KNOWLEDGE.md`)

> **Primary Objective**: This document serves as the complete, authoritative, and exhaustive technical reference for the **Moon programming language**, its compiler pipeline, runtime virtual machine, memory management systems, standard library implementations, developer tooling (LSP / REPL), build architecture, and WebAssembly web playground integration.
> 
> *When prompting an AI assistant or onboarding an engineer, reading this single file completely and accurately restores full comprehension of the entire codebase and its design invariants.*
> 
> ⚠️ **MANDATORY KNOWLEDGE RECONCILIATION INVARIANT**:
> Whenever new features, semantics, optimizations, bug fixes, design decisions, architectural changes, standard library methods, or runtime behaviors are discovered, created, or learned anywhere in the Moon language ecosystem (`moon` or `moon-web`), **they MUST be immediately and accurately reconciled into this `KNOWLEDGE.md` file**. This file is a living specification and must always remain the single, 100% comprehensive source of truth.

---

## Table of Contents
1. [Executive Summary & Core Design Philosophy](#1-executive-summary--core-design-philosophy)
2. [Language Syntax & Semantics](#2-language-syntax--semantics)
3. [Lexical Analysis & Scanner Mechanics](#3-lexical-analysis--scanner-mechanics)
4. [Pratt Parser & Phrasal Signature DFA](#4-pratt-parser--phrasal-signature-dfa)
5. [Abstract Syntax Tree & Memory Safety](#5-abstract-syntax-tree--memory-safety)
6. [Bytecode Instruction Set Architecture & Codegen](#6-bytecode-instruction-set-architecture--codegen)
7. [Virtual Machine & Direct-Threaded Dispatch](#7-virtual-machine--direct-threaded-dispatch)
8. [Value Representation, Object Model & Hash Table](#8-value-representation-object-model--hash-table)
9. [Dynamic Type System & Universal Type Casting](#9-dynamic-type-system--universal-type-casting)
10. [Garbage Collection & Memory Management](#10-garbage-collection--memory-management)
11. [Standard Library Architecture & Native Linker](#11-standard-library-architecture--native-linker)
12. [Developer Tooling, LSP Server & REPL Engine](#12-developer-tooling-lsp-server--repl-engine)
13. [Build System, Makefile & Dual-Repository Workflow](#13-build-system-makefile--dual-repository-workflow)
14. [Complete Reference & Quick-Start Cheat Sheet](#14-complete-reference--quick-start-cheat-sheet)

---

## 1. Executive Summary & Core Design Philosophy

**Moon** is an expressive, high-performance, dynamic programming language implemented in pure C (C99). It is designed to combine the readability and natural cadence of human language with the computational rigor, strict scoping, and execution speed of a bytecode-compiled virtual machine.

### Key Tenets
1. **Natural Phrasal Syntax**: Functions can be defined and called using multi-word natural phrases (e.g. `calculate (x) plus (y) times (z)`, `add (x) to (y)`, `reverse list`, `split string by ","`), without sacrificing formal Chomsky grammar hierarchy or introducing ambiguous parsing lookaheads.
2. **Human-Centric Ergonomics**:
   - **1-Based Indexing & Negative Slicing**: Indexing starts at `1` (`list[1]` is the first element), while negative indexing accesses elements from the tail (`list[-1]` is the last element). Slicing uses `list[start to end]`.
   - **Backtick String Interpolation**: String interpolation is embedded cleanly within standard double quotes using backticks: `"Hello, `name`! You have `score + 10` points."`.
   - **Implicit Pronouns & Sticky Subjects**: The `it` keyword refers to the active expression subject (e.g., `if count > 5 and it < 10:`).
   - **Chained Comparisons**: Mathematical inequalities like `1 < x <= 10` evaluate without duplicating side effects via a dedicated VM sticky stack.
   - **Multi-Dispatch & Dynamic Blueprints**: Methods and phrasal functions support polymorphic multiple dispatch based on runtime type signatures.
3. **High-Performance C Engine**:
   - **Zero-Allocation Scanner**: Yields pointer+length token slices directly from source memory.
   - **Top-Down Operator Precedence (Pratt) Parser**: Fast, deterministic AST construction.
   - **Deterministic Phrasal DFA Signature Trie**: Maps natural multi-word phrases to mangled bytecode entry points in $O(K)$ time (where $K$ is the number of phrase words).
   - **Direct-Threaded Bytecode VM**: Uses GCC/Clang computed gotos for near-native instruction dispatch.
   - **NaN-Boxing**: All 64-bit Moon `Value`s fit inside an IEEE 754 double precision register (floats, booleans, nil, and 48-bit heap pointers).
   - **Iterative Mark-and-Sweep GC**: Eliminates C call-stack recursion overflows by utilizing an explicit gray stack worklist for traversal.

---

## 2. Language Syntax & Semantics

*(Derived strictly from the C compiler codebase and verified `.moon` test suites).*

### 2.1 Comments
Single-line comments begin with `#` and continue to the end of the line:
```moon
# This is a single-line comment
let score be 100 # Inline comment
```

#### Multiline Comments (`##`)
Moon features elegant, indentation-based multiline comments. A multiline comment block begins with `##`. All subsequent lines that are indented past the column of the `##` are treated as part of the multiline comment. No closing delimiter or tag is required:
```moon
## MULTILINE COMMENT
   This whole block is ignored by the compiler.
   As long as subsequent lines are indented,
   they are safely consumed as part of the comment.

show "This executes normally!"
```

### 2.2 Variables & Assignment
Variables are declared with `let ... be ...` and mutated with `set ... to ...` or `update ... +/- ...`:
```moon
# Single and multi-word variable declarations
let x be 42
let user name be "Ada Lovelace"
let isActive be true

# Multi-variable declarations (supporting both single and spaced identifiers)
let first name, last name be "Munachiso", "Ukpai"
let a, middle name, c be 1, "Grace", 3
let player score, high score be 100 # Broadcast single value to multiple variables

# Mutation
set x to 100
set user name to "Grace Hopper"
set first name, last name to "Munachi", "U."

# In-place update
update x + 5      # x is now 105
update x - 10     # x is now 95
```

### 2.3 Literals & Primitive Types
- **Numbers**: Standard 64-bit IEEE 754 floating point: `0`, `42`, `-15.8`, `1e6`.
- **Booleans**: `true`, `false`.
- **Nil**: `nil` (represents absence of value).
- **Strings**: Double-quoted UTF-8 text with escape characters (`\n`, `\t`, `\"`, `\\`, `` \` ``) and interpolation:
  ```moon
  let name be "Moon"
  let greeting be "Welcome to `name` language!"
  ```

### 2.4 Collections: Lists, Dictionaries & Ranges
```moon
# Lists (Dynamic, 1-indexed)
let numbers be [10, 20, 30, 40, 50]
show numbers[1]          # Output: 10 (1-based index)
show numbers[-1]         # Output: 50 (last element)
show numbers[2 to 4]     # Slicing: [20, 30, 40]
set numbers[1] to 99     # Mutation

# Dictionaries (Hash tables with String/Value keys)
let user be {
  name: "Alan",
  age: 41,
  account balance: 5000
}
show user["name"]
show user["account balance"]
set user["age"] to 42

# Ranges
let r be 1 to 10          # Range 1..10 (step 1)
let rStep be 1 to 10 by 2 # Range 1, 3, 5, 7, 9
```

### 2.5 List & Dictionary Comprehensions
Comprehensions use `for each` and `keep` to emit elements:
```moon
# List comprehension
let evens be [ for each n in 1 to 10 keep n * 2 ]
# evens is [2, 4, 6, 8, 10, 12, 14, 16, 18, 20]

# List comprehension with filtering (using 'mod')
let numbers be [1, 2, 3, 4, 5, 6]
let filtered be [ for each n in numbers keep n if n mod 2 == 0 ]
# filtered is [2, 4, 6]

# Dictionary comprehension
let squares be { for each n in 1 to 5 keep n: n * n }
# squares is {1: 1, 2: 4, 3: 9, 4: 16, 5: 25}
```

### 2.6 Control Flow
#### Conditional Statements (`if`, `unless`)
```moon
let score be 85
let isActive be false

# Multi-line if/else
if score >= 90:
  show "Grade: A"
else if score >= 80:
  show "Grade: B"
else:
  show "Grade: C"
end

# unless (negated condition)
unless isActive:
  show "Account is suspended."
end

# Single-line if statement
if score > 50 show "Passed!"
```

#### Loops (`while`, `until`, `for`)
```moon
# while loop
let count be 1
while count <= 5:
  show "Count: `count`"
  update count + 1
end

# until loop (equivalent to while not condition)
until count == 0:
  update count - 1
end

# for loop over range / list
for i from 1 to 5:
  show i
end

for item in ["alpha", "beta", "gamma"]:
  show item
end

# Loop control: break and skip (continue)
for i in 1 to 10:
  if i == 3:
    skip
  end
  if i == 8:
    break
  end
  show i
end
```

### 2.7 Functions & Natural Phrasal Calls
#### Standard Phrasal Functions
Functions are declared using `let <phrase>:` and returned with `give`:
```moon
# Phrasal function definition
let calculate (x) plus (y) times (z):
  give x + (y * z)
end

# Phrasal call (clean natural syntax)
let result be calculate 5 plus 3 times 4
show result # Output: 17
```

#### Infix / Leading-Argument Phrases
Phrasal functions can accept a leading argument before the root verb:
```moon
let (s: String) repeated (n: Number) times:
  let out be ""
  let i be 0
  while i < n:
    set out to out + s
    set i to i + 1
  end
  give out
end

let repeatedStr be "Moon! " repeated 3 times
show repeatedStr # Output: "Moon! Moon! Moon! "
```

#### Custom Operator Phrases
```moon
let (n: Number) + (days: Number) days:
  give n + (days * 86400)
end

let timestamp be 1000 + 5 days
show timestamp # Output: 433000
```

### 2.8 Type Blueprints & Object-Oriented Semantics
```moon
# Blueprint definition
type Player:
  name is "Anonymous",
  health is 100,
  speed is 10,
  
  # Method inside Blueprint
  take damage (amount):
    set my health to my health - amount
    if my health < 0:
      set my health to 0
    end
  end
end

# Instantiation
let p1 be Player {
  name: "Hero",
  health: 150
}

# Accessing fields via possessive 's
show p1's name    # Output: "Hero"
show p1's health  # Output: 150

# Calling methods
p1's take damage 30
show p1's health  # Output: 120

# Possessive Extension Methods (defined outside Blueprint)
let (p: Player)'s heal (amount):
  set p's health to p's health + amount
end

p1's heal 10
show p1's health  # Output: 130
```

### 2.9 Universal Type Casting (`as`)
```moon
type Player:
  name is "Anonymous",
  health is 100
end

let numStr be "12345"
let num be numStr as Number       # 12345

let pi be 3.14159
let piStr be pi as String         # "3.14159"

# Hydrating Dictionaries into Type Blueprints
let rawData be { name: "Bob", health: 80 }
let p2 be rawData as Player
show p2's name                    # "Bob"
```

---

## 3. Lexical Analysis & Scanner Mechanics

**Source**: `src/scanner.h`, `src/scanner.c`

### 3.1 Token Data Structure
The scanner operates with zero heap allocations during tokenization. Tokens point directly into the source string buffer:
```c
typedef struct {
  TokenType type;
  const char *start;        // Pointer to start of token slice
  int length;               // Byte length of token
  int line;                 // 1-based line number
  int column;               // 1-based horizontal column
  const char *errorMessage; // Error string if type == TOKEN_ERROR
} Token;
```

### 3.2 Keyword Recognition (Trie Switch)
The scanner identifies 35 reserved keywords using an unrolled trie switch for maximum speed:
```
Keywords: add, and, as, be, by, break, each, else, end, false, for, from, 
          give, if, in, is, it, keep, let, load, nil, not, or, quit, 
          set, skip, then, to, true, type, unless, until, update, while, with
```

### 3.3 String Interpolation State Machine
String interpolation in Moon supports arbitrary nested expressions inside backticks:
- Lexer encounters `"Hello ` -> emits `TOKEN_STRING_OPEN`, increments `scanner.interpolationDepth`.
- Lexer enters code mode: scans normal tokens (variables, operators, function calls).
- Lexer encounters closing backtick:
  - If more string follows (`` ` world ` ``) -> emits `TOKEN_STRING_MIDDLE`.
  - If string closes (`` `!" ``) -> emits `TOKEN_STRING_CLOSE`, decrements `scanner.interpolationDepth`.
- The parser groups these into a `NODE_INTERPOLATION` AST node.

### 3.4 Colon Rule & Multi-line Block Indentation
In Moon, a colon `:` that is the last non-whitespace character on a line signifies the start of a code block (equivalent to `{` in C). The REPL uses `calculateBlockDepth()` to dynamically calculate block nesting:
```c
int calculateBlockDepth(const char *source);
```

---

## 4. Pratt Parser & Phrasal Signature DFA

**Source**: `src/parser.h`, `src/parser.c`, `src/sigtrie.h`, `src/sigtrie.c`

### 4.1 Operator Precedence Hierarchy
Moon employs Top-Down Operator Precedence (Pratt Parsing) with 11 distinct precedence levels:
```c
typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // = be to
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == != is
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * / %
  PREC_UNARY,       // not - !
  PREC_CALL,        // . () [] 's
  PREC_PRIMARY
} Precedence;
```

### 4.2 Pratt Parse Rule Table
Each token maps to a `ParseRule` containing a prefix parselet, an infix parselet, and its infix precedence:
```c
typedef Node *(*PrefixFn)(Token token);
typedef Node *(*InfixFn)(Node *left, Token token);

typedef struct {
  PrefixFn prefix;
  InfixFn infix;
  Precedence precedence;
} ParseRule;
```

### 4.3 Phrasal Signature Trie DFA (`sigtrie.c`)
To enable arbitrary multi-word natural phrases without causing LL(k) / LR(k) grammar explosion, Moon registers all phrasal signatures into a prefix trie DFA:
```c
typedef enum { NODE_LABEL, NODE_ARGUMENT } PhraseNodeType;
typedef enum { TERMINAL_NONE, TERMINAL_PHRASE, TERMINAL_VARIABLE } TerminalType;

typedef struct TrieNode {
  PhraseNodeType type;
  uint32_t labelHash;
  char *labelName;
  int labelLength;
  int arity;
  bool isLeadingArg;
  TerminalType terminalType;
  bool isCore;
  char *mangledName;
  struct TrieNode **children;
  int childCount;
  int childCapacity;
} TrieNode;
```

#### Mangling Schema
| Source Phrasal Signature | Mangled Function Identifier |
| :--- | :--- |
| `calculate (x) plus (y) times (z)` | `calculate$1_plus$1_times$1` |
| `add (x) to (y)` | `add$1_to$1` |
| `split (s) by (d)` | `split$1by$1` |
| `read file (path)` | `read#file$1` |
| `numbers in (seq) in base (b)` | `numbers#in$1in#base$1` |
| `(s: String) repeated (n: Number) times` | `$1_repeated$1_times` |

#### Disambiguation Strategy
When the parser encounters an identifier, it queries the `phrasalTable` and `propertyTable` hash tables:
1. If the word starts a registered phrasal signature, it advances the DFA across argument expressions and intervening label tokens.
2. If it is a multi-word variable (e.g. `user score`), the parser checks ahead to avoid consuming following statements.
3. If no phrasal signature matches, it treats the identifier as a standard variable reference.

---

## 5. Abstract Syntax Tree & Memory Safety

**Source**: `src/ast.h`, `src/ast.c`

### 5.1 Tagged Union Node Structure
Every AST element is represented by a uniform `Node` (`struct sNode`) structure:
```c
struct sNode {
  NodeType type;
  Node *parent;
  int line;
  bool usesIt;    // Implicit pronoun propagation flag
  union {
    LiteralPayload literal;
    VariablePayload variable;
    BinaryPayload binary;
    UnaryPayload unary;
    ChainPayload chain;
    RangePayload range;
    ListPayload list;
    DictPayload dict;
    SubscriptPayload subscript;
    PropertyPayload property;
    PhrasalCallPayload phrasalCall;
    PhrasalMethodCallPayload phrasalMethodCall;
    ExtensionMethodPayload extensionMethod;
    InterpolationPayload interpolation;
    SetPayload set;
    IfPayload ifStmt;
    WhilePayload whileStmt;
    ForPayload forStmt;
    BlockPayload block;
    LetPayload let;
    FunctionPayload function;
    TypeDeclPayload typeDecl;
    InstantiatePayload instantiate;
    CastPayload cast;
    UnionTypePayload unionType;
    ComprehensionPayload comprehension;
    KeepPayload keepStmt;
  } as;
};
```

### 5.2 Implicit Subject (`usesIt`) Propagation
During AST generation, if an expression refers to the `it` keyword, `node->usesIt = true` bubbles up recursively to its parent statements. The code generator uses this to automatically emit `OP_LOAD_STICKY` and bind the sticky subject without programmer intervention.

### 5.3 Iterative DFS Memory Reclamation (`freeNode`)
Recursive AST destruction in C can easily overflow the execution stack on deep ASTs or cyclic graphs. Moon implements an iterative worklist-based DFS traversal in `freeNode(Node *root)`:
```c
void freeNode(Node *node) {
  // Uses an internal heap-allocated dynamic worklist stack to free
  // all child nodes iteratively, guaranteeing O(1) C stack consumption.
}
```

---

## 6. Bytecode Instruction Set Architecture & Codegen

**Source**: `src/chunk.h`, `src/chunk.c`, `src/codegen.h`, `src/codegen.c`, `src/emitter.h`, `src/emitter.c`

### 6.1 Complete Opcode Set (60 Instructions)
```c
typedef enum {
  OP_CONSTANT,         // [const_idx] -> pushes constant value
  OP_CONSTANT_LONG,    // [idx_byte1, idx_byte2, idx_byte3] -> pushes constant
  OP_PUSH_BYTE,        // [byte] -> pushes small immediate integer
  OP_NIL,              // pushes nil
  OP_TRUE,             // pushes true
  OP_FALSE,            // pushes false
  OP_POP,              // pops 1 value from stack
  OP_POP_N,            // [count] -> pops N values from stack (scope cleanup)
  OP_GET_LOCAL,        // [slot] -> pushes local variable from stack slot
  OP_SET_LOCAL,        // [slot] -> updates local variable at slot
  OP_GET_LOCAL_LONG,   // [slot_24bit]
  OP_SET_LOCAL_LONG,   // [slot_24bit]
  OP_GET_GLOBAL,       // [name_idx] -> pushes global variable
  OP_DEFINE_GLOBAL,    // [name_idx] -> defines global in current module table
  OP_SET_GLOBAL,       // [name_idx] -> updates global in module table
  OP_ADD,              // [a, b] -> a + b (Numbers or String Ropes)
  OP_ADD_INPLACE,      // in-place addition optimization
  OP_SUBTRACT,         // [a, b] -> a - b
  OP_MULTIPLY,         // [a, b] -> a * b
  OP_DIVIDE,           // [a, b] -> a / b
  OP_MOD,              // [a, b] -> a % b
  OP_NEGATE,           // [a] -> -a
  OP_NOT,              // [a] -> !a
  OP_EQUAL,            // [a, b] -> a == b
  OP_GREATER,          // [a, b] -> a > b
  OP_LESS,             // [a, b] -> a < b
  OP_JUMP,             // [offset_16bit] -> unconditional jump forward
  OP_JUMP_IF_FALSE,    // [offset_16bit] -> conditional jump forward
  OP_LOOP,             // [offset_16bit] -> jump backward to loop start
  OP_BUILD_STRING,     // [count] -> concatenates top N stack items into Rope
  OP_BUILD_LIST,       // [count] -> creates ObjList from top N stack items
  OP_BUILD_DICT,       // [count] -> creates ObjDict from top 2*N stack items
  OP_BUILD_UNION,      // [count] -> creates ObjUnion from top N type blueprints
  OP_GET_PROPERTY,     // [name_idx] -> reads property/method from instance
  OP_SET_PROPERTY,     // [name_idx] -> writes property to instance
  OP_GET_SUBSCRIPT,    // [target, index] -> reads index or slice
  OP_SET_SUBSCRIPT,    // [target, index, val] -> writes index
  OP_GET_END_INDEX,    // calculates 1-based length for negative index resolution
  OP_RANGE,            // [start, end, step] -> creates ObjRange
  OP_GET_ITER,         // converts sequence to iterator
  OP_FOR_ITER,         // [jump_offset] -> advances iterator or jumps
  OP_FOR_ITER_LONG,    // 24-bit jump offset version
  OP_GET_ITER_VALUE,   // unpacks current iterator value
  OP_GET_ITER_VALUE_LONG,
  OP_CALL,             // [arg_count, cache_idx] -> invokes function
  OP_INVOKE,           // [name_idx, arg_count, cache_idx] -> invokes method
  OP_TYPE_DEF,         // [name_idx, prop_count] -> declares ObjType blueprint
  OP_INSTANTIATE,      // [prop_count] -> creates ObjInstance from blueprint
  OP_DEFINE_METHOD,    // registers method into ObjType or ObjMultiFunction
  OP_DEFINE_EXTENSION_METHOD, // registers method on foreign blueprint
  OP_CAST,             // [type_val] -> coerces value to requested type
  OP_LOAD,             // loads foreign Moon module file
  OP_KEEP_LIST,        // comprehension list accumulator
  OP_KEEP_DICT,        // comprehension dict accumulator
  OP_RETURN,           // returns from current CallFrame
  OP_PUSH_SEQUENCE,    // saves active sequence register
  OP_PUSH_STICKY,      // pushes active comparison subject to stickyStack
  OP_POP_STICKY,       // pops from stickyStack
  OP_SET_STICKY,       // updates current sticky register
  OP_LOAD_STICKY,      // pushes current sticky subject to VM stack
  OP_SHOW_REPL         // pretty-prints expression result in REPL mode
} OpCode;
```

### 6.2 Scope Management & Local Resolution
The compiler tracks lexical scopes using `Compiler` structs linked via `enclosing` pointers:
- Variables are stored in a contiguous `locals` array: `Local locals[UINT8_COUNT]`.
- On scope exit (`endScope()`), `OP_POP_N` is emitted to reclaim all local slots at once.

### 6.3 Inline Caches (`InlineCacheEntry`)
Every call instruction (`OP_CALL`, `OP_INVOKE`) holds an inline cache slot:
```c
typedef struct {
  CacheType type;
  ObjType *receiverType;
  ObjType *argTypes[MAX_CACHE_ARGS];
  Value cachedValue;
} InlineCacheEntry;
```
When a method or overloaded function is invoked, the VM caches the resolved `ObjFunction` pointer for the receiver and argument types. Subsequent calls with matching types execute with zero table lookup overhead ($O(1)$ dispatch).

---

## 7. Virtual Machine & Direct-Threaded Dispatch

**Source**: `src/vm.h`, `src/vm.c`

### 7.1 VM State Architecture
```c
#define FRAMES_MAX 32768
#define STACK_MAX (FRAMES_MAX * 256)

typedef struct {
  ObjFunction *function;
  uint8_t *ip;
  Value *slots;
  Table *globals;
  Value stickySubject;
} CallFrame;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frameCount;

  Value stack[STACK_MAX];
  Value *stackTop;

  Table globals;
  Table strings;        // Interned string pool
  Table loadedModules;  // Module cache

  Obj *objects;         // GC root linked list

  // Native Type Registry Blueprints
  ObjType *anyType;
  ObjType *typeType;
  ObjType *numberType;
  ObjType *stringType;
  ObjType *listType;
  ObjType *dictType;
  ObjType *boolType;
  ObjType *rangeType;
  ObjType *functionType;
  ObjType *nilType;
  ObjType *moduleType;

  // GC State
  size_t bytesAllocated;
  size_t nextGC;
  Obj **grayStack;
  int grayCount;
  int grayCapacity;
  bool allowGC;

  Value stickyStack[256];
  int stickyCount;
} VM;
```

### 7.2 Computed Gotos Execution Loop
When compiled with GCC or Clang, `vm.c` uses Direct Threaded Code (Computed Gotos) via `&&code_OP_*` labels and `goto *dispatchTable[*ip++]`. This bypasses branch prediction bottlenecks found in traditional `switch` loops:
```c
#if defined(__GNUC__) || defined(__clang__)
  #define DISPATCH() goto *dispatchTable[*frame->ip++]
#else
  #define DISPATCH() goto run_switch
#endif
```

### 7.3 Chained Comparisons & Sticky Stack
When evaluating `1 < x <= 10`:
1. `1` is evaluated.
2. `x` is evaluated and pushed to `vm.stickyStack`.
3. `1 < x` is evaluated. If `false`, short-circuits.
4. If `true`, the VM emits `OP_LOAD_STICKY` to reload `x` without re-evaluating any expressions.
5. `x <= 10` is evaluated.
6. `OP_POP_STICKY` restores the previous sticky subject context.

---

## 8. Value Representation, Object Model & Hash Table

**Source**: `src/value.h`, `src/value.c`, `src/object.h`, `src/object.c`, `src/table.h`, `src/table.c`

### 8.1 NaN-Boxing (64-Bit Value Encoding)
Moon represents all runtime values in a single 64-bit IEEE 754 float:
```
64-bit Float: [Sign: 1] [Exponent: 11 (all 1s)] [Quiet Bit: 1] [Payload: 51 bits]
```
- **Numbers**: Any non-quiet-NaN bit pattern.
- **Specials**:
  - `QNAN | TAG_NIL`   $\to$ `nil`
  - `QNAN | TAG_FALSE` $\to$ `false`
  - `QNAN | TAG_TRUE`  $\to$ `true`
  - `QNAN | SIGN_BIT | 48-bit pointer` $\to$ Heap `Obj*` pointer.

```c
#define QNAN ((uint64_t)0x7ffc000000000000)
#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define TAG_NIL 1
#define TAG_FALSE 2
#define TAG_TRUE 3

#define IS_NUMBER(val) (((val).asBits & QNAN) != QNAN)
#define IS_OBJ(val) (((val).asBits & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))
```

### 8.2 Heap Object Types (`ObjKind`)
```c
typedef enum {
  OBJ_STRING,          // String with Rope tree support
  OBJ_RANGE,           // Numeric range (start, end, step)
  OBJ_LIST,            // Dynamic array of Values
  OBJ_DICT,            // Hash table dictionary
  OBJ_FUNCTION,        // Compiled bytecode chunk
  OBJ_NATIVE,          // C function pointer wrapper
  OBJ_TYPE_BLUEPRINT,  // Prototype blueprint (Type)
  OBJ_INSTANCE,        // Instantiated object with fields
  OBJ_MULTI_FUNCTION,  // Multiple-dispatch overloaded method table
  OBJ_UNION,           // Union type (e.g. String or Number)
  OBJ_MODULE           // Module namespace
} ObjKind;
```

#### String Ropes (`ObjString`)
Strings in Moon support both flat character buffers and binary Rope trees (`left`, `right`). Concatenations (`OP_ADD`) construct tree nodes in $O(1)$ time, postponing linear buffer allocation until `flattenString()` is called.

### 8.3 Open-Addressing Hash Table (`table.c`)
- Uses linear probing with cached FNV-1a hash keys.
- Tombstones are used to preserve probe sequences during deletions.
- Dynamic resizing triggers at 75% load factor (`TABLE_MAX_LOAD = 0.75`).

---

## 9. Dynamic Type System & Universal Type Casting

**Source**: `src/cast.h`, `src/cast.c`

### 9.1 Type Blueprints vs Instances
- An `ObjType` acts as a blueprint containing property names, default values, and method definitions.
- An `ObjInstance` points to its blueprint (`type`) and holds an instance-specific `fields` hash table.
- Property lookups check `instance->fields` first, falling back to `type->properties`.

### 9.2 Universal Casting Rules (`cast.c`)
The `as` operator (`OP_CAST`) performs the following conversions:
1. **To `String`**: Converts numbers, booleans, nil, lists, and dicts to string representation.
2. **To `Number`**: Parses numeric strings (supports base conversion via `numbers in str in base N`).
3. **To `List`**: Converts ranges to lists; extracts keys/values from dictionaries.
4. **To `Dict`**: Converts pairs list `[[k1, v1], [k2, v2]]` into a dictionary.
5. **To `Blueprint`**: Hydrates a dictionary into a structured object instance.
6. **To `Union`**: Verifies that the operand matches at least one type in the union.

---

## 10. Garbage Collection & Memory Management

**Source**: `src/memory.h`, `src/memory.c`

Moon features a precise, non-recursive **Mark-and-Sweep Garbage Collector**.

### 10.1 Root Set Enumeration
1. **VM Stack**: All active values in `vm.stack` from `vm.stack` to `vm.stackTop`.
2. **Call Frames**: Active `CallFrame` functions and `homeGlobals`.
3. **Global Tables**: `vm.globals`, `vm.loadedModules`.
4. **Sticky Stacks**: `vm.stickyStack`, `vm.sequenceStack`.
5. **Native Type Registry**: Builtin type blueprints (`vm.anyType`, `vm.stringType`, etc.).

### 10.2 Non-Recursive Gray Stack Tracing
To prevent C call-stack overflows on deeply nested or cyclic structures, `markObject()` pushes objects onto an explicit heap-allocated `vm.grayStack`. The tracing loop in `traceReferences()` iteratively processes this worklist:
```c
static void traceReferences() {
  while (vm.grayCount > 0) {
    Obj *object = vm.grayStack[--vm.grayCount];
    blackenObject(object);
  }
}
```

### 10.3 Sweep Phase
Traverses the intrusive singly-linked list `vm.objects`:
- If `obj->isMarked == true`: Clears the bit (`obj->isMarked = false`) for the next GC cycle.
- If `obj->isMarked == false`: Unlinks the object, frees its payload buffers, and reclaims memory.

### 10.4 Adaptive GC Threshold
```c
vm.bytesAllocated += bytes;
if (vm.bytesAllocated > vm.nextGC) {
  collectGarbage();
}
// After sweep:
vm.nextGC = vm.bytesAllocated * GC_GROWTH_FACTOR; // GC_GROWTH_FACTOR = 2
```

---

## 11. Standard Library Architecture & Native Linker

**Source**: `src/lib_core.c`, `src/lib_string.c`, `src/lib_list.c`, `src/lib_math.c`, `src/lib_io.c`

### 11.1 Native Registration Macro (`REGISTER_PHRASE`)
Natives are registered into the Signature Trie and Global Scope during VM boot:
```c
#define REGISTER_PHRASE(module, root, path, arity, mangledName, fn, ...) \
  do { \
    ObjType *types[] = {__VA_ARGS__}; \
    registerNativePhrasal(module, root, path, arity, mangledName, fn, \
                          (arity) > 0 ? types : NULL); \
  } while (0)
```

### 11.2 Standard Modules

#### Core (`lib_core.c`)
- `show (val)` $\to$ `show$1`: Prints stringified value with newline.
- `ask (prompt)` $\to$ `ask$1`: Prompts user and reads line from stdin (or browser prompt in WASM).
- `clock` $\to$ `clock`: Returns high-resolution seconds elapsed.
- `memory` $\to$ `memory`: Returns dictionary containing memory metrics (`used`, `limit`, object counts).
- `gc` $\to$ `gc`: Forces a garbage collection cycle and returns reclaimed byte count.

#### String (`lib_string.c`)
- `uppercase (str)` $\to$ `uppercase$1`
- `lowercase (str)` $\to$ `lowercase$1`
- `trim (str)` $\to$ `trim$1`
- `split (str) by (delim)` $\to$ `split$1by$1` (UTF-8 multi-byte character boundary aware).

#### List (`lib_list.c`)
- `reverse (list)` $\to$ `reverse$1`
- `join (list) with (delim)` $\to$ `join$1with$1`
- `pop from (list)` $\to$ `pop#from$1`
- `shift from (list)` $\to$ `shift#from$1` (Hardware-accelerated `memmove`).
- `numbers in (seq) in base (b)` $\to$ `numbers#in$1in#base$1` (Horner's method).
- `index of (item) in (list)` $\to$ `index#of$1in$1` (1-based index).

#### Math (`lib_math.c`)
- `sin (x)`, `cos (x)`
- `square root of (x)` $\to$ `square#root#of$1`
- `power of (base) to (exp)` $\to$ `power#of$1to$1`
- `floor of (x)` $\to$ `floor#of$1`
- `random from (min) to (max)` $\to$ `random#from$1to$1`

#### IO (`lib_io.c`)
- `read file (path)` $\to$ `read#file$1`
- `write (content) to (path)` $\to$ `write$1to$1`
- `append (content) to file (path)` $\to$ `append$1to#file$1`
- `file (path) exists` $\to$ `file$1exists`

#### Standard Library In Action
```moon
# String operations
let upper be uppercase "moon"
let words be split "alpha,beta,gamma" by ","
let joined be join words with " - "

# List operations
let numbers be [1, 2, 3]
let rev be reverse numbers
let idx be index of 2 in numbers

# Math operations
let root be square root of 25
let pwr be power of 2 to 8
let rounded be floor of 4.9

# Time
let now be clock
```

---

## 12. Developer Tooling, LSP Server & REPL Engine

**Source**: `src/lsp.h`, `src/lsp.c`, `src/error.h`, `src/error.c`, `src/main.c`

### 12.1 Language Server Protocol (`lsp.c`)
Moon includes a built-in Language Server Protocol (LSP) engine supporting Neovim, VSCode, and other LSP clients:
- **Transport**: JSON-RPC over `stdin`/`stdout`.
- **Capabilities**:
  - `textDocument/didOpen`, `textDocument/didChange`: Incremental document synchronization.
  - `textDocument/publishDiagnostics`: Live syntax and reference error squiggles.
  - **Spellchecking Oracle**: Computes Levenshtein distance against known symbols to suggest corrections (`"Did you mean 'score'?"`).
  - `textDocument/semanticTokens/full`: High-accuracy semantic token highlighting.
  - `textDocument/hover`: Displays function signatures and preceding docstrings (`extractDocstring()`).
  - `textDocument/completion`: Scope-aware completions for variables, phrasal functions, and blueprint properties.

### 12.2 Compiler & Runtime Error Engine (`error.c`)
- **ANSI Color Palette**: Stylized terminal error messages with line numbers, code snippets, and pointing carets (`^`).
- **LSP Bridge**: When `isLspMode = true`, errors are redirected to `lspDiagnostics` instead of printing to stdout/stderr.

### 12.3 Interactive REPL & CLI Debugging (`main.c`)
- **Smart Block Depth Calculator**: Automatically detects open blocks (`:`, `{`, `[`) and draws visual tree branch prompt indicators:
  ```
  moon > for i in 1 to 3:
        ├─> show i
        ├─> end
  ```
- **CLI Debug Flags**:
  Always use the executable's debug tags `--xx` during development to gain full diagnostic visibility into each compiler and VM phase:
  - `--scan`: Dumps scanner token stream (lexical analysis) with escaped control characters (`\n`, `\t`, `\r`, `\\`). Does **not** execute the program unless `-r` / `--run` is passed.
  - `--ast`: Prints Abstract Syntax Tree hierarchy (parser output). Does **not** execute the program unless `-r` / `--run` is passed.
  - `--sigtrie`: Dumps signature trie DFA states and active phrase registrations. Does **not** execute the program unless `-r` / `--run` is passed.
  - `--bytecode`: Disassembles compiled bytecode chunks with line mapping and constants. Does **not** execute the program unless `-r` / `--run` is passed.
  - `--vm`: Enables live instruction-by-instruction VM stack tracing during execution. Stack traces display concise function representations (`<name$arity (Type1, ...)>`).
  - `-r` / `--run`: Forces program execution when combined with inspection flags (e.g. `./moon file.moon --scan -r`).
  - `--no-run`: Explicitly compiles and validates syntax/semantics without executing bytecode in the VM.
  - `--lsp`: Starts the JSON-RPC Language Server Protocol service.

> [!TIP]
> Diagnostic inspection flags (`--scan`, `--ast`, `--sigtrie`, `--bytecode`) default to non-executing. Combine them with `-r` (e.g. `./moon --scan -r file.moon`) to inspect and execute in a single command. Use `--vm` for runtime opcode and stack visualization.

- **Object Stringification & Debug Signatures**:
  Multi-functions and phrasal signatures format cleanly without verbose type wrappers:
  - Single parameter: `<show$1 (Any)>`
  - Multiple parameters: `<add$2 (Number, Number)>`
  - Multiple overloads: `<add$2 (Number, Number) | (String, Any)>`
  - Union parameters: `<process$1 (List or String)>`

---

## 13. Build System, Makefile & Dual-Repository Workflow

### 13.1 Makefile Targets
The Moon engine root directory contains a standalone `Makefile`:
```makefile
CC = gcc
CFLAGS = -g -std=c99 -Wall -Wextra -I./src -MMD -MP
```
- `make` (or `make all`): Builds the `moon` debug binary with full debug symbols and dependency tracking (`.d` files).
- `make release`: Rebuilds the binary with `-O3` optimizations.
- `make test`: Cleans, builds, and executes the complete automated Python test suite (`python3 scripts/test_harness.py`).
- `make clean`: Removes the `moon` executable, `.o` object files, and `.d` dependency files.

### 13.2 WebAssembly Build Pipeline (`./build_wasm.sh`)
Moon compiles to WebAssembly for execution in the browser via Emscripten:
```bash
#!/bin/bash
source /home/emrys/emsdk/emsdk_env.sh
emcc src/*.c -o /home/emrys/moon-web/src/moon.js \
    -s MODULARIZE=1 \
    -s EXPORT_NAME="MoonModule" \
    -s EXPORTED_RUNTIME_METHODS='["cwrap", "ccall"]' \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ASYNCIFY=1 \
    -s EXPORTED_FUNCTIONS='["_executeMoonCode", "_initMoonWeb", "_setCompilerFlags", "_malloc", "_free"]' \
    -s EXPORT_ES6=1 \
    -O3 -I./src
```
- **Asyncify**: Enables synchronous C calls like `askNative()` to suspend WASM execution and await asynchronous JavaScript input from the React web interface without freezing the browser event loop.
- **Output**: Generates `moon.js` and `moon.wasm` directly into `moon-web/src/`.

### 13.3 End-to-End Development, Testing & Git Workflow
When implementing new features or bug fixes in Moon, follow this exact step-by-step workflow:

1. **Modify C Code**: Make necessary changes in `/home/emrys/moon/src/*.c` or `src/*.h`.
2. **Add New Tests**: Create new `.moon` test files under `/home/emrys/moon/tests/` (e.g., `tests/syntax/`, `tests/core/`, `tests/stdlib/`, `tests/gc/`). Every test must contain `# expect: <output>` assertion comments:
   ```moon
   # tests/syntax/test_my_feature.moon
   let x be 10 + 20
   show x
   # expect: 30
   ```
3. **Run Native Tests**: In `/home/emrys/moon`, execute:
   ```bash
   make test
   ```
   Ensure all tests pass with 0 failures.
4. **Compile WebAssembly**: In `/home/emrys/moon`, execute:
   ```bash
   ./build_wasm.sh
   ```
5. **Validate Web Frontend**: In `/home/emrys/moon-web`, verify the build:
   ```bash
   npm run build
   ```
   (Optionally run `npm run dev` to test the interactive playground in the browser).
6. **Reconcile Knowledge Base**: If any new syntax, standard library function, compiler feature, VM behavior, or architectural design was added, modified, or learned, update `/home/emrys/moon/KNOWLEDGE.md` to reflect the changes before committing.
7. **Commit Moon Repository**:
   ```bash
   cd /home/emrys/moon
   git add .
   git commit -m "feat(core): implement feature with tests"
   ```
8. **Commit Moon-Web Repository**:
   ```bash
   cd /home/emrys/moon-web
   git add .
   git commit -m "chore(wasm): update moon wasm bundle"
   ```

---

## 14. Complete Reference & Quick-Start Cheat Sheet

### Syntax Cheat Sheet
```moon
# Declarations & Mutations
let var name be 42
set var name to 100
update var name + 10

# Output & Time
show "Value is `var name`"
let currentTime be clock

# Conditionals
let x be 15
if x > 10:
  show "High"
else if x > 5:
  show "Medium"
else:
  show "Low"
end

# Loops
for i in 1 to 5:
  show i
end

let count be 3
while count > 0:
  update count - 1
end

# Functions & Methods
let multiply (a) by (b):
  give a * b
end

type Counter:
  value is 0,
  increment:
    set my value to my value + 1
  end
end

let c be Counter {}
c's increment
show c's value # 1
```

### Memory Inspection Recipes
```moon
# Query memory statistics (zero-argument phrasal call)
let mem be memory
show "Allocated bytes: `mem's used`"
show "Total objects: `mem's total_objects`"

# Force GC cycle
let freed be gc
show "Reclaimed `freed` bytes."
```
