// compiler.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

// ==========================================
// PHRASAL FUNCTION TRIE (COMPILE-TIME ONLY)
// ==========================================

typedef struct TrieNode {
  uint32_t hash;      // The hashed token label (e.g., hash("from"))
  int segmentArity;   // Args immediately following this word
  int totalArity;     // Total args for the final VM call
  bool isTerminal;    // True if this ends a valid phrase
  char *stitchedName; // The name the VM sees: "move_from_to"

  // Dynamic array of child nodes
  int childCount;
  int childCapacity;
  struct TrieNode **children;
} TrieNode;

// The global root of our signature tree
TrieNode *signatureTrieRoot = NULL;

// Helper to create a new node
static TrieNode *newTrieNode(uint32_t hash) {
  TrieNode *node = ALLOCATE(TrieNode, 1);
  node->hash = hash;
  node->segmentArity = 0;
  node->totalArity = 0;
  node->isTerminal = false;
  node->stitchedName = NULL;
  node->childCount = 0;
  node->childCapacity = 0;
  node->children = NULL;
  return node;
}

// Memory cleanup for the Trie (Runs after compilation finishes)
static void freeTrie(TrieNode *node) {
  if (node == NULL)
    return;

  for (int i = 0; i < node->childCount; i++) {
    freeTrie(node->children[i]);
  }

  FREE_ARRAY(TrieNode *, node->children, node->childCapacity);

  if (node->stitchedName != NULL) {
    // Assuming stitchedName was allocated via standard malloc/ALLOCATE
    free(node->stitchedName);
  }

  FREE(TrieNode, node);
}

// ==========================================
// TOKEN CACHE (PASS 0 PIPELINE)
// ==========================================

Token *tokenStream = NULL;
int tokenCount = 0;
int tokenCapacity = 0;
int currentTokenIndex = 0;

typedef struct {
  Token name;
  int depth;
  bool isCaptured; // <--- Add this line
} Local;

// Helper struct to manage temporary compilation buffers
typedef struct {
  Chunk *previousChunk; // The "Real" chunk we paused
  Chunk tempChunk;      // The "Draft" chunk we are writing to
} ScopedChunk;

typedef enum { TYPE_FUNCTION, TYPE_SCRIPT } FunctionType;

typedef struct Compiler {
  struct Compiler *enclosing; // Parent compiler (e.g., script -> function)
  ObjFunction *function;      // The function being compiled
  FunctionType type;          // Is this the main script or a user function?

  Chunk *chunk;

  Local locals[UINT8_COUNT];
  int localCount;
  int scopeDepth;
} Compiler;

static Token syntheticToken(const char *text) {
  Token token;
  token.type = TOKEN_IDENTIFIER; // Make sure it's an ID
  token.start = text;
  token.length = (int)strlen(text);
  token.line = 0; // Internal
  return token;
}

Compiler *current = NULL;

// Start Drafting: Redirects compiler output to a temp chunk
static void beginScopeChunk(ScopedChunk *scope) {
  scope->previousChunk = current->chunk;

  // This MUST zero out constants, code, and capacity
  initChunk(&scope->tempChunk);
  current->chunk = &scope->tempChunk;
}

// End Drafting: Restores compiler output to the main chunk
static void endScopeChunk(ScopedChunk *scope) {
  current->chunk = scope->previousChunk; // Restore the main scroll
  // Note: We don't free tempChunk here; the caller must use (flush) or discard
  // it.
}

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_RANGE,
  PREC_OR,         // or
  PREC_AND,        // and
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)();
typedef void (*StatementFn)();

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

Parser parser;

static int resolveLocal(Compiler *compiler, Token *name); // <--- ADD THIS
static void emitByte(uint8_t byte);
static void emitBytes(uint8_t byte1, uint8_t byte2);
static Chunk *currentChunk() { return current->chunk; }

// -- Error Handling --

static void errorAt(Token *token, const char *message) {
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

static void error(const char *message) { errorAt(&parser.previous, message); }

static void errorAtCurrent(const char *message) {
  errorAt(&parser.current, message);
}

// -- Token Advancement --

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    // Pull the next token from our pre-lexed array!
    parser.current = tokenStream[currentTokenIndex++];

    if (parser.current.type != TOKEN_ERROR)
      break;

    errorAtCurrent(parser.current.start);
  }
}

static void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool checkTerminator(TokenType *terminators, int count) {
  for (int i = 0; i < count; i++)
    if (check(terminators[i]))
      return true;

  return check(TOKEN_EOF); // Always stop at EOF to prevent infinite loops
}

static bool match(TokenType type) {
  if (!check(type))
    return false;

  advance();
  return true;
}

static void ignoreNewlines() {
  while (match(TOKEN_NEWLINE)) {
    // Just consume them and do nothing.
  }
}

// -- Bytecode Emission --

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitReturn() { emitBytes(OP_NIL, OP_RETURN); }

static uint8_t makeConstant(Value value) {
  // ALWAYS add constants to the real function chunk,
  // even if we are drafting code to a temp chunk.
  int constant = addConstant(&current->function->chunk, value);

  if (constant > 255) {
    error("Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t)constant;
}

static void emitConstant(Value value) {
  emitBytes(OP_CONSTANT, makeConstant(value));
}

// Emits a jump instruction with 2 placeholder bytes (0xff 0xff)
// Returns the offset of the placeholder so we can patch it later.
static int emitJump(uint8_t instruction) {
  emitByte(instruction);
  emitByte(0xff);
  emitByte(0xff);
  return currentChunk()->count - 2;
}

// Goes back to 'offset' and writes the current location difference
static void patchJump(int offset) {
  // -2 to adjust for the jump offset itself
  int jump = currentChunk()->count - offset - 2;

  if (jump > UINT16_MAX) {
    error("Too much code to jump over.");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

// Emits a backward jump (loop)
static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);

  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX)
    error("Loop body too large.");

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}

// -- Forward Declarations --

// Forward declarations
static void expression();
static void statement();
static void declaration();
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

// -- Variables & Identifiers --

static uint8_t identifierConstant(Token *name) {
  return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

// CLEANED: Logic is now purely for reading (getting) a variable.
// The "set" logic is handled in setStatement().
static void namedVariable(Token name) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);

  if (arg != -1) {
    getOp = OP_GET_LOCAL;
  } else {
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
  }

  emitBytes(getOp, (uint8_t)arg);
}

static void variable() { namedVariable(parser.previous); }

// -- Literals --

static void number() {
  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

// Copies string source but unescapes double backticks
static ObjString *parseStringFromToken(Token token) {
  // We assume the token includes the delimiters ("...", "...`, `...`, `...")
  // We need to strip 1 char from start and 1 char from end.
  return copyString(token.start + 1, token.length - 2);
}

static void string() {
  // 1. Emit the first part (Literal)
  Token *t = &parser.previous;
  // Note: We use +1/-2 to strip the quotes ("...")
  emitConstant(OBJ_VAL(copyStringUnescaped(t->start + 1, t->length - 2)));

  int partCount = 1; // We start with 1 string on the stack

  // 2. Loop through interpolations
  while (parser.previous.type == TOKEN_STRING_OPEN ||
         parser.previous.type == TOKEN_STRING_MIDDLE) {

    // Compile the expression inside `...`
    expression();
    partCount++; // Pushes the expression result

    if (partCount > 255)
      error("Too many parts in interpolated string.");

    // Advance to the next chunk of the string
    advance();

    if (parser.previous.type == TOKEN_STRING_MIDDLE) {
      emitConstant(OBJ_VAL(parseStringFromToken(parser.previous)));
      partCount++; // Pushes the string literal
    } else if (parser.previous.type == TOKEN_STRING_CLOSE) {
      emitConstant(OBJ_VAL(parseStringFromToken(parser.previous)));
      partCount++; // Pushes the string literal
      break;       // Done
    } else {
      error("Expect end of string interpolation.");
    }
  }

  // 3. Emit the Build Instruction
  // If it's a simple string ("hello"), partCount is 1. We don't need to build.
  // If it's interpolated ("hi `name`"), partCount > 1.
  if (partCount > 1) {
    emitBytes(OP_BUILD_STRING, (uint8_t)partCount);
  }
}

static void literal() {
  switch (parser.previous.type) {
  case TOKEN_FALSE:
    emitByte(OP_FALSE);
    break;
  case TOKEN_NIL:
    emitByte(OP_NIL);
    break;
  case TOKEN_TRUE:
    emitByte(OP_TRUE);
    break;
  default:
    return; // Unreachable.
  }
}

// -- Expressions --

static void grouping() {
  ignoreNewlines(); // Allow: ( \n 1 + 2 )

  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary() {
  TokenType operatorType = parser.previous.type;

  // Compile the operand.
  parsePrecedence(PREC_UNARY);

  // Emit the operator instruction.
  switch (operatorType) {
  case TOKEN_MINUS:
    emitByte(OP_NEGATE);
    break;
  case TOKEN_NOT:
    emitByte(OP_NOT);
    break; // <--- ADD THIS LINE
  default:
    return; // Unreachable.
  }
}

static void binary() {
  TokenType operatorType = parser.previous.type;
  ParseRule *rule = getRule(operatorType);

  // -- The "is not" Fix --
  bool invert = false;
  if (operatorType == TOKEN_IS && match(TOKEN_NOT)) {
    invert = true;
  }
  // ----------------------

  // Ignore newlines
  ignoreNewlines();

  // Compile the right-hand operand.
  // We use the precedence of the current operator ('is') plus one.
  parsePrecedence((Precedence)(rule->precedence + 1));

  switch (operatorType) {
  // Equality
  case TOKEN_IS:
  case TOKEN_EQUAL_EQUAL:
  case TOKEN_EQUAL:
    emitByte(OP_EQUAL);
    if (invert)
      emitByte(OP_NOT); // x is not y -> !(x == y)
    break;

  // Comparison
  case TOKEN_GREATER:
    emitByte(OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    emitBytes(OP_LESS, OP_NOT);
    break;
  case TOKEN_LESS:
    emitByte(OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    emitBytes(OP_GREATER, OP_NOT);
    break;

  // Arithmetic
  case TOKEN_PLUS:
    emitByte(OP_ADD);
    break;
  case TOKEN_MINUS:
    emitByte(OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emitByte(OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emitByte(OP_DIVIDE);
    break;

  default:
    return; // Unreachable.
  }
}

static void and_() {
  // Left operand is already on stack.
  // If left is false, short-circuit (jump to end).
  int endJump = emitJump(OP_JUMP_IF_FALSE);

  emitByte(OP_POP); // Pop left operand (it was true)

  ignoreNewlines();

  parsePrecedence(PREC_AND); // Compile right operand
  patchJump(endJump);
}

static void or_() {
  // Left operand is on stack.
  // Logic: "Jump if False" is standard. "Jump if True" requires a slightly
  // different logic. Easier way: Jump if False to the *next* check. If it
  // didn't jump, it means it was True.

  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  int endJump = emitJump(OP_JUMP);

  patchJump(elseJump);
  emitByte(OP_POP); // Pop left (it was false)

  ignoreNewlines();

  parsePrecedence(PREC_OR);
  patchJump(endJump);
}

static uint8_t argumentList() {
  uint8_t argCount = 0;

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      ignoreNewlines(); // Allow: call( \n a, \n b )

      expression();
      if (argCount >= 255)
        error("Can't have more than 255 arguments.");
      argCount++;
    } while (match(TOKEN_COMMA));
  }

  ignoreNewlines(); // Allow trailing newline before ')'
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");

  return argCount;
}

static void call() {
  uint8_t argCount = argumentList();
  emitBytes(OP_CALL, argCount);
}

// A temporary container for snatched bytecode
typedef struct {
  uint8_t *code;
  int *lines;
  int count;
} CodeBuffer;

typedef enum { TARGET_LOCAL, TARGET_GLOBAL, TARGET_COMPLEX } TargetType;

typedef struct {
  TargetType type;
  Token rootName;
  int arg;              // Holds local slot or global constant index
  CodeBuffer drillCode; // Holds the snipped index math (e.g., [0][1])
} SetTarget;

// Moves code from the current chunk into a buffer
static void snipCode(CodeBuffer *buffer, int startOffset) {
  Chunk *chunk = currentChunk();

  // Calculate how many bytes we are moving
  buffer->count = chunk->count - startOffset;

  if (buffer->count == 0)
    return; // Nothing to snip

  // Allocate memory for the buffer
  buffer->code = (uint8_t *)malloc(buffer->count * sizeof(uint8_t));
  buffer->lines = (int *)malloc(buffer->count * sizeof(int));

  // Copy the raw bytes and line numbers
  // Note: We assume chunk->lines is a simple array matching chunk->code 1:1
  memcpy(buffer->code, chunk->code + startOffset, buffer->count);
  memcpy(buffer->lines, chunk->lines + startOffset,
         buffer->count * sizeof(int));

  // Rewind the chunk!
  // We effectively "delete" the bytes from the main program
  chunk->count = startOffset;
}

// Writes the buffered code back into the current chunk
static void pasteCode(CodeBuffer *buffer) {
  if (buffer->count == 0)
    return;

  for (int i = 0; i < buffer->count; i++) {
    writeChunk(currentChunk(), buffer->code[i], buffer->lines[i]);
  }

  // Clean up
  free(buffer->code);
  free(buffer->lines);
}

static void list() {
  int itemCount = 0;

  // 1. Allow newline immediately after opening bracket
  // [ \n 1, 2 ]
  ignoreNewlines();

  if (!check(TOKEN_RIGHT_BRACKET)) {
    do {
      // 2. Allow newline before an element (and after a comma)
      // [ 1, \n 2 ]
      ignoreNewlines();

      // Check for trailing comma followed by closing bracket: [ 1, \n ]
      if (check(TOKEN_RIGHT_BRACKET)) {
        break;
      }

      expression();

      if (itemCount == 255) {
        error("Can't have more than 255 items in a list.");
      }
      itemCount++;
    } while (match(TOKEN_COMMA));
  }

  // 3. Allow newline before the closing bracket
  // [ 1, 2 \n ]
  ignoreNewlines();

  consume(TOKEN_RIGHT_BRACKET, "Expect ']' after list.");
  emitBytes(OP_BUILD_LIST, (uint8_t)itemCount);
}

static void addLocal(Token name) {
  if (current->localCount == UINT8_COUNT) {
    error("Too many local variables in function.");
    return;
  }

  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1; // Uninitialized
}

static void markInitialized() {
  if (current->scopeDepth == 0)
    return;

  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void subscript() {
  // 1. THE ANCHOR: Bind the list/string on the stack to '.target'
  addLocal(syntheticToken(".target"));
  current->locals[current->localCount - 1].depth = current->scopeDepth;

  markInitialized();

  // 2. Parse the math (e.g., 'end - 1' or '1 to end')
  expression();

  consume(TOKEN_RIGHT_BRACKET, "Expect ']' after index.");

  // 3. THE CLEANUP: Un-bind the anchor so it doesn't leak
  current->localCount--;

  // 4. Fetch the item!
  emitByte(OP_GET_SUBSCRIPT);
}

static void dot() {
  parsePrecedence(PREC_CALL);
  emitByte(OP_GET_SUBSCRIPT);
}

static void possessive() {
  consume(TOKEN_IDENTIFIER, "Expect property name after 's.");

  // Treat the identifier as a literal string key!
  uint8_t name = identifierConstant(&parser.previous);
  emitBytes(OP_CONSTANT, name);

  emitByte(OP_GET_SUBSCRIPT);
}

static void range() {
  // 1. Compile the Right-Hand Side of 'to' (The End value)
  // We use PREC_TERM so it correctly evaluates math like '1 to 5 + 2'
  parsePrecedence(PREC_TERM);

  // 2. The 'by' Check
  if (match(TOKEN_BY)) {
    // The user provided a step! Compile it so it goes onto the stack.
    parsePrecedence(PREC_TERM);
  } else {
    // 3. The Sneaky Injection
    // The user didn't provide a step. We secretly emit a '1' onto the stack!
    // (Assuming your emitConstant uses NUMBER_VAL macro for doubles)
    emitConstant(NUMBER_VAL(1.0));
  }

  // 4. Emit the final instruction
  // The VM stack now perfectly contains: [Start] [End] [Step]
  emitByte(OP_RANGE);
}

static void endKeyword() {
  // 1. Look for the hidden anchor
  Token targetName = syntheticToken(".target");
  int arg = resolveLocal(current, &targetName);

  if (arg == -1) {
    // If it's not found, they typed 'end' in normal math outside of brackets
    error("Can only use 'end' inside a list index or slice.");
    return;
  }

  // 2. Push a copy of the anchored list/string to the top of the stack
  emitBytes(OP_GET_LOCAL, (uint8_t)arg);

  // 3. Tell the VM to replace that object with its exact length
  emitByte(OP_GET_END_INDEX);
}

// -- The Rules Table --

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},

    // -- String Rules --
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_STRING_OPEN] = {string, NULL, PREC_NONE},
    [TOKEN_STRING_MIDDLE] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING_CLOSE] = {NULL, NULL, PREC_NONE},

    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_IS] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_NOT] = {unary, NULL, PREC_NONE},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},

    [TOKEN_IF] = {NULL, NULL, PREC_ASSIGNMENT},

    // The '[' token does double duty!
    [TOKEN_LEFT_BRACKET] = {list, subscript, PREC_CALL},
    [TOKEN_POSSESSIVE] = {NULL, possessive, PREC_CALL},

    [TOKEN_RIGHT_BRACKET] = {NULL, NULL, PREC_NONE},

    // The '.' token handles shorthand indexing
    [TOKEN_DOT] = {NULL, dot, PREC_CALL},

    [TOKEN_TO] = {NULL, range, PREC_RANGE},
    [TOKEN_BY] = {NULL, NULL, PREC_NONE},
    [TOKEN_END] = {endKeyword, NULL, PREC_NONE},

    [TOKEN_NEWLINE] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *getRule(TokenType type) { return &rules[type]; }

static void parsePrecedence(Precedence precedence) {
  advance();

  // 1. Mark the start of this expression
  // If this becomes the LHS of a ternary, we will cut from here.
  int exprStart = currentChunk()->count;

  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    error("Expect expression.");
    return;
  }

  prefixRule();

  while (precedence <= getRule(parser.current.type)->precedence) {

    // --- START OF TERNARY LOGIC ---
    if (parser.current.type == TOKEN_IF) {

      // Lookahead: Is this a Ternary or a Guard?
      if (!checkTernary()) {
        // No 'else' found. It's a Statement Guard.
        // We STOP parsing expressions here.
        // This returns control to statement(), which handles the guard.
        return;
      }

      // It IS a Ternary. Consume 'if'.
      advance();

      // A. Snip the LHS (True Branch)
      CodeBuffer lhsBuffer;
      snipCode(&lhsBuffer, exprStart);

      // B. Compile the Condition
      expression();

      // C. Emit Jump over the True Branch (to Else)
      int jumpToElse = emitJump(OP_JUMP_IF_FALSE);
      emitByte(OP_POP); // Clean the condition from stack

      // D. Paste the LHS (True Branch)
      pasteCode(&lhsBuffer);

      // E. Emit Jump over the Else Branch (to End)
      int jumpToEnd = emitJump(OP_JUMP);

      // F. Compile the Else Branch
      patchJump(jumpToElse);
      emitByte(OP_POP); // Clean condition (for the False path)

      consume(TOKEN_ELSE, "Expect 'else' after ternary condition.");
      expression();

      // G. Finish
      patchJump(jumpToEnd);

      continue; // Continue parsing (allows chaining)
    }
    // --- END OF TERNARY LOGIC ---

    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule();
  }
}

static void initCompiler(Compiler *compiler, FunctionType type) {
  compiler->enclosing = current; // Link to the previous global 'current'
  compiler->function = NULL;
  compiler->type = type;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;

  compiler->function = newFunction();

  current = compiler; // Make this the new 'current' compiler
  compiler->chunk = &compiler->function->chunk;

  if (type != TYPE_SCRIPT) {
    current->function->name =
        copyString(parser.previous.start, parser.previous.length);
  }

  Local *local = &compiler->locals[compiler->localCount++];
  local->depth = 0;
  local->isCaptured = false;
  local->name.start = "";
  local->name.length = 0;
}

static ObjFunction *endCompiler() {
  emitReturn(); // Emit OP_NIL, OP_RETURN for the end of the function

  ObjFunction *function = current->function;

#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), function->name != NULL
                                         ? function->name->chars
                                         : "<script>");
  }
#endif

  current = current->enclosing; // Restore the parent compiler
  return function;
}

static void beginScope() { current->scopeDepth++; }

static void endScope() {
  current->scopeDepth--;

  // Pop locals that fell out of scope
  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > current->scopeDepth) {
    emitByte(OP_POP);
    current->localCount--;
  }
}

static bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;

  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer.");
      }

      return i;
    }
  }

  return -1;
}

static void declareVariable() {
  if (current->scopeDepth == 0)
    return;

  Token *name = &parser.previous;
  // Check for redeclaration in same scope
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }
    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }

  addLocal(*name);
}

static uint8_t parseVariable(const char *errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);
  declareVariable();
  if (current->scopeDepth > 0)
    return 0;

  return identifierConstant(&parser.previous);
}

static void defineVariable(uint8_t global) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }
  emitBytes(OP_DEFINE_GLOBAL, global);
}

// Parse a block of statements.
// If 'canEndWithElse' is true, the block stops if it sees 'else' or 'end'.
// If false, it MUST find 'end'.
static void block(TokenType *terminators, int count) {
  // We loop indefinitely until we hit a terminator or EOF
  while (true) {
    // 1. Skip empty lines inside the block
    ignoreNewlines();

    // 2. Check if we are done (e.g., hit 'end' or 'else' or '}')
    if (checkTerminator(terminators, count))
      return;

    // 3. Safety: Stop at EOF to prevent infinite loops
    if (check(TOKEN_EOF))
      return;

    // 4. Parse the next statement
    declaration();
  }
}

static void consumeEnd() {
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

static void expression() { parsePrecedence(PREC_ASSIGNMENT); }

// -- Statement Parsing --

static void showBody() {
  expression();
  emitByte(OP_PRINT);
  // remove consumeEnd() if you added it here earlier!
}

static void expressionBody() {
  expression();
  emitByte(OP_POP);
  // remove consumeEnd() if you added it here earlier!
}

// Helper to check for a variable name token and return its constant index
static uint8_t parseVariableConstant(const char *errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);
  return identifierConstant(&parser.previous);
}

static void setBody() {
  // =========================================================================
  // PHASE 1: THE SETUP (Parsing the Left-Hand Side)
  // =========================================================================
  SetTarget targets[255];
  int targetCount = 0;

  do {
    if (targetCount >= 255) {
      error("Can't set more than 255 variables in one statement.");
    }

    SetTarget *target = &targets[targetCount++];

    // 1. The Root Variable
    target->rootName = parser.current;
    consume(TOKEN_IDENTIFIER, "Expect variable name.");

    // 2. The Fork: Complex vs Simple
    if (check(TOKEN_LEFT_BRACKET) || check(TOKEN_DOT) ||
        check(TOKEN_POSSESSIVE)) {
      target->type = TARGET_COMPLEX;

      int drillStart = currentChunk()->count;

      // Drill Down Loop
      while (check(TOKEN_LEFT_BRACKET) || check(TOKEN_DOT) ||
             check(TOKEN_POSSESSIVE)) {
        TokenType type = parser.current.type;

        if (type == TOKEN_LEFT_BRACKET) {
          advance();
          expression();
          consume(TOKEN_RIGHT_BRACKET, "Expect ']' after subscript.");
        } else if (type == TOKEN_DOT) {
          advance();
          parsePrecedence(PREC_CALL);
        } else if (type == TOKEN_POSSESSIVE) {
          advance(); // Consume the 's
          consume(TOKEN_IDENTIFIER, "Expect property name after 's.");
          uint8_t name = identifierConstant(&parser.previous);
          emitBytes(OP_CONSTANT, name);
        }

        if (check(TOKEN_LEFT_BRACKET) || check(TOKEN_DOT) ||
            check(TOKEN_POSSESSIVE)) {
          emitByte(OP_GET_SUBSCRIPT);
        }
      }

      snipCode(&target->drillCode, drillStart);

    } else {
      // Simple Variable Logic
      int arg = resolveLocal(current, &target->rootName);
      if (arg != -1) {
        target->type = TARGET_LOCAL;
        target->arg = arg;
      } else {
        target->type = TARGET_GLOBAL;
        target->arg = identifierConstant(&target->rootName);
      }
      target->drillCode.count = 0; // Mark as empty for safety
    }

  } while (match(TOKEN_COMMA));

  consume(TOKEN_TO, "Expect 'to' after variable name(s).");

  // =========================================================================
  // PHASE 2: THE ANCHORING (Parsing the Right-Hand Side)
  // =========================================================================
  int valCount = 0;

  do {
    if (current->localCount >= 255) {
      error("Too many variables in scope to perform parallel assignment.");
    }

    expression(); // Evaluates the value and pushes it to the VM stack

    // SECURE THE VALUE: Claim a hidden local variable slot for it
    addLocal(syntheticToken(".temp"));
    markInitialized();
    valCount++;

  } while (match(TOKEN_COMMA));

  // Validation Check
  if (valCount > 1 && valCount != targetCount) {
    error("Value count does not match variable count.");
  }

  // Find where our hidden locals start on the stack
  int tempBaseSlot = current->localCount - valCount;

  // =========================================================================
  // PHASE 3: THE WEAVE (Assembling the Bytecode)
  // =========================================================================
  for (int i = 0; i < targetCount; i++) {
    SetTarget *target = &targets[i];

    // Determine which hidden value slot this target should use
    int tempSlot = tempBaseSlot + (valCount == 1 ? 0 : i);

    if (target->type == TARGET_COMPLEX) {
      // --- THE COMPLEX WEAVE ---

      // A. Load the Root Object
      int rootArg = resolveLocal(current, &target->rootName);
      if (rootArg != -1) {
        emitBytes(OP_GET_LOCAL, (uint8_t)rootArg);
      } else {
        emitBytes(OP_GET_GLOBAL, identifierConstant(&target->rootName));
      }

      // B. Paste the sniped index code (e.g., the math for `[0][1]`)
      pasteCode(&target->drillCode); // (This automatically frees the buffer)

      // C. Retrieve our securely anchored value
      emitBytes(OP_GET_LOCAL, (uint8_t)tempSlot);

      // D. Strike!
      emitByte(OP_SET_SUBSCRIPT);

      // E. Cleanup the setter result (leaves the value on stack, we pop it)
      emitByte(OP_POP);

      // (Step F deleted!)
    } else {
      // --- THE SIMPLE WEAVE ---

      // A. Retrieve our securely anchored value
      emitBytes(OP_GET_LOCAL, (uint8_t)tempSlot);

      // B. Strike!
      uint8_t setOp =
          (target->type == TARGET_LOCAL) ? OP_SET_LOCAL : OP_SET_GLOBAL;
      emitBytes(setOp, (uint8_t)target->arg);

      // C. Cleanup the setter result
      emitByte(OP_POP);
    }
  }

  // =========================================================================
  // CLEANUP: Destroy the Anchors
  // =========================================================================
  // We must explicitly tell the VM to pop the original evaluated values
  // off the stack, and we must remove them from the compiler's tracker.
  for (int i = 0; i < valCount; i++) {
    emitByte(OP_POP);
    current->localCount--;
  }
}

// 'let x be v'
// 'let x, y be 3, 4' or 'let x, y be 3'
static void function(FunctionType type);
static void varDeclaration() {
  uint8_t globals[255]; // Temporary array to hold global variable names
  int varCount = 0;     // Tracks how many variables are on the Left-Hand Side

  // =========================================================================
  // STEP 2: PARSE THE LHS (Read Commas for Variables)
  // =========================================================================
  do {
    if (varCount >= 255) {
      error("Can't declare more than 255 variables in one statement.");
    }

    if (current->scopeDepth > 0) {
      // Local Variable Logic
      consume(TOKEN_IDENTIFIER, "Expect variable name.");
      Token *name = &parser.previous;

      // Check for redeclaration in same scope
      for (int i = current->localCount - 1; i >= 0; i--) {
        Local *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth)
          break;
        if (identifiersEqual(name, &local->name)) {
          error("Already a variable with this name in this scope.");
        }
      }
      addLocal(*name);
      globals[varCount] = 0; // Dummy value, locals don't use this array
    } else {
      // Global Variable Logic
      globals[varCount] = parseVariableConstant("Expect variable name.");
    }

    varCount++;

    // -- FUNCTION DECLARATION CHECK --
    // If we parsed exactly ONE variable and immediately see '(', it's a
    // function. e.g., 'let add(a, b):'
    if (varCount == 1 && check(TOKEN_LEFT_PAREN)) {
      if (current->scopeDepth > 0)
        markInitialized();

      function(
          TYPE_FUNCTION); // This compiles the function and pushes it to stack

      if (current->scopeDepth == 0) {
        emitBytes(OP_DEFINE_GLOBAL, globals[0]);
        emitByte(OP_POP); // Clean the stack! (Since OP_DEFINE_GLOBAL now peeks)
      }
      return; // We are completely done parsing this statement.
    }

  } while (match(TOKEN_COMMA)); // Keep looping if we see commas!

  // =========================================================================
  // STEP 3: PARSE THE RHS (Read Commas for Values)
  // =========================================================================
  if (match(TOKEN_BE)) {
    ignoreNewlines();

    int valCount = 0;
    do {
      expression(); // Compiles the value and pushes it to the stack
      valCount++;
    } while (match(TOKEN_COMMA));

    // =========================================================================
    // STEP 4: VALIDATION
    // =========================================================================
    if (valCount > 1 && valCount != varCount) {
      error("Value count does not match variable count.");
    }

    consumeEnd();

    // =========================================================================
    // STEP 5: EMISSION AND STACK ALIGNMENT
    // =========================================================================
    if (current->scopeDepth > 0) {
      // --- LOCAL VARIABLES ALIGNMENT ---
      if (valCount == 1 && varCount > 1) {
        // Many-to-1 (let x, y be 3)
        // '3' is on the stack and naturally fell into x's local slot.
        // We use OP_GET_LOCAL to copy x's slot into the remaining variables!
        int firstLocalSlot = current->localCount - varCount;
        for (int i = 1; i < varCount; i++) {
          emitBytes(OP_GET_LOCAL, firstLocalSlot);
        }
      }
      // Finally, mark all these newly filled locals as fully initialized
      for (int i = 0; i < varCount; i++) {
        current->locals[current->localCount - 1 - i].depth =
            current->scopeDepth;
      }

    } else {
      // --- GLOBAL VARIABLES ALIGNMENT ---
      if (valCount == 1) {
        // Many-to-1 (let x, y be 3)
        // Stack has [ 3 ]. Assign it to all variables backwards, then pop it.
        for (int i = varCount - 1; i >= 0; i--) {
          emitBytes(OP_DEFINE_GLOBAL, globals[i]);
        }
        emitByte(OP_POP); // Clean up the single value
      } else {
        // 1-to-1 Mapping (let x, y be 3, 4)
        // Stack has [ 3 ] [ 4 ]. Assign backwards, popping as we go.
        for (int i = varCount - 1; i >= 0; i--) {
          emitBytes(OP_DEFINE_GLOBAL, globals[i]);
          emitByte(OP_POP); // Clean up each value immediately after defining
        }
      }
    }

  } else {
    // =========================================================================
    // EDGE CASE: UNINITIALIZED VARIABLES (let x, y)
    // =========================================================================
    // They implicitly default to 'nil'
    for (int i = 0; i < varCount; i++) {
      emitByte(OP_NIL);
    }

    consumeEnd();

    if (current->scopeDepth > 0) {
      for (int i = 0; i < varCount; i++) {
        current->locals[current->localCount - 1 - i].depth =
            current->scopeDepth;
      }
    } else {
      for (int i = varCount - 1; i >= 0; i--) {
        emitBytes(OP_DEFINE_GLOBAL, globals[i]);
        emitByte(OP_POP);
      }
    }
  }
}

static void ifStatement();
static void ifLogic(bool invert) {
  expression(); // Condition

  // 2. Invert if needed
  if (invert) {
    emitByte(OP_NOT);
  }

  // 1. The 'THEN' Block
  if (match(TOKEN_COLON)) {
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);

    // Define terminators for THEN block: 'else' or 'end'
    TokenType thenEnds[] = {TOKEN_ELSE, TOKEN_END};
    block(thenEnds, 2);

    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP);

    // 2. The 'ELSE' Block
    if (match(TOKEN_ELSE)) {
      if (match(TOKEN_IF)) {
        ifStatement(); // Recursion for 'else if'
      } else if (match(TOKEN_COLON)) {
        // Define terminators for ELSE block: strictly 'end'
        TokenType elseEnds[] = {TOKEN_END};
        block(elseEnds, 1);

        consume(TOKEN_END, "Expect 'end' after else block.");
      } else {
        statement(); // Single line else
      }
    } else {
      // If there was no else, we MUST consume the 'end' that stopped the block
      consume(TOKEN_END, "Expect 'end' after if block.");
    }

    patchJump(elseJump);
  }

  // 3. Single Statement Syntax (if x > y show x)
  else {
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE)) {
      statement(); // Single statement else
    }
    patchJump(elseJump);
  }
}

static void ifStatement() {
  ifLogic(false); // Normal 'if'
}

static void unlessStatement() {
  ifLogic(true); // 'unless' is just 'if' with a flipped condition
}

static void loopLogic(bool invert) {
  // 1. Mark the loop start (so we can jump back here)
  int loopStart = currentChunk()->count;

  // 2. Compile the Condition
  expression();

  // The flip
  if (invert) {
    emitByte(OP_NOT);
  }

  // 3. Jump logic (Exit if condition is false)
  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP); // Pop condition (if true)

  // 4. Compile the Body
  if (match(TOKEN_COLON)) {
    // Block Syntax: while x > 0: ... end
    TokenType terminators[] = {TOKEN_END};
    block(terminators, 1);

    // We expect a strict 'end' here
    consume(TOKEN_END, "Expect 'end' after loop.");
  } else {
    // Single Statement Syntax: while x > 0 show x
    statement();
  }

  // 5. Loop back to start
  emitLoop(loopStart);

  // 6. Patch the exit jump
  patchJump(exitJump);
  emitByte(OP_POP); // Pop condition (if false)
}

static void whileStatement() { loopLogic(false); }

static void untilStatement() { loopLogic(true); }

// Error synchronization (skips tokens until a safe point)
static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_END)
      return; // Moon uses 'end' mostly?

    switch (parser.current.type) {
    case TOKEN_LET:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_SHOW:
    case TOKEN_GIVE:
    case TOKEN_SET: // Added set as a sync point
      return;
    default:;
    }

    advance();
  }
}

static void function(FunctionType type) {
  // 1. Create a new Compiler for this function
  Compiler compiler;
  initCompiler(&compiler, type);

  // 2. Enter Scope (Parameters are local variables)
  beginScope();

  // 3. Parse Parameters: (a, b)
  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        errorAtCurrent("Can't have more than 255 parameters.");
      }
      // Force add parameter as local variable
      uint8_t constant = parseVariable("Expect parameter name.");
      defineVariable(constant);
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

  // 4. Parse Body
  if (match(TOKEN_COLON)) {
    // Block Syntax: let add(a,b): ... end
    // We stop at 'end'.
    TokenType terminators[] = {TOKEN_END};
    block(terminators, 1);

    consume(TOKEN_END, "Expect 'end' after function body.");
  } else {
    // Single Statement Syntax: let add(a,b) give a+b
    statement();
  }

  // 5. Create Function Object
  ObjFunction *function = endCompiler();

  // 6. Emit the function constant in the parent script
  emitBytes(OP_CONSTANT, makeConstant(OBJ_VAL(function)));

  // (Note: The caller `declaration` handles defining the variable name)
}

static void declaration() {
  // 1. Gobble any leading newlines
  ignoreNewlines();

  // 2. Safety Check: Stop at EOF
  if (check(TOKEN_EOF))
    return;

  // 3. The Routing
  if (match(TOKEN_LET)) {
    varDeclaration(); // <--- Plugged in here!
  } else {
    statement();
  }

  // 4. Panic Mode Recovery
  if (parser.panicMode)
    synchronize();
}

// Helper to parse declarations until a specific terminator is found
static void executeBlock(TokenType terminator, const char *errorMessage) {
  while (!check(terminator) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(terminator, errorMessage);
}

static void returnBody() {
  if (current->type == TYPE_SCRIPT) {
    error("Can't return from top-level code.");
  }

  // Check for "Empty Return" (give followed by terminator)
  // We use check() because we MUST NOT eat the newline here.
  // statementWithGuard needs it later.
  if (check(TOKEN_NEWLINE) || check(TOKEN_EOF) || check(TOKEN_END) ||
      check(TOKEN_ELSE) || check(TOKEN_RIGHT_BRACE)) {

    emitReturn(); // Emits OP_NIL, OP_RETURN
  } else {
    // Return a value
    expression();
    emitByte(OP_RETURN);
  }
}

static void statementWithGuard(StatementFn bodyFn) {
  ScopedChunk scope;
  beginScopeChunk(&scope);
  bodyFn();
  endScopeChunk(&scope); // <--- Make sure this is called!

  if (match(TOKEN_IF) || match(TOKEN_UNLESS)) {
    TokenType guardType = parser.previous.type;

    expression();

    if (guardType == TOKEN_UNLESS)
      emitByte(OP_NOT);

    int jump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);

    Chunk *temp = &scope.tempChunk;
    for (int i = 0; i < temp->count; i++) {
      writeChunk(currentChunk(), temp->code[i], temp->lines[i]);
    }

    int doneJump = emitJump(OP_JUMP);

    patchJump(jump);
    emitByte(OP_POP);

    patchJump(doneJump);
  } else {
    Chunk *temp = &scope.tempChunk;

    for (int i = 0; i < temp->count; i++) {
      writeChunk(currentChunk(), temp->code[i], temp->lines[i]);
    }
  }

  FREE_ARRAY(int, scope.tempChunk.lines, scope.tempChunk.capacity);
  FREE_ARRAY(uint8_t, scope.tempChunk.code, scope.tempChunk.capacity);

  consumeEnd();
}

static void forStatement() {
  beginScope();

  if (check(TOKEN_EACH))
    advance();

  Token iteratorName = parser.current;
  consume(TOKEN_IDENTIFIER, "Expect variable name.");
  consume(TOKEN_IN, "Expect 'in' after variable name.");

  expression(); // [Sequence]

  emitByte(OP_GET_ITER); // [Iterator Start]

  addLocal(syntheticToken(" (sequence)"));
  markInitialized();
  addLocal(syntheticToken(" (iterator)"));
  markInitialized();

  int loopStart = currentChunk()->count;

  emitByte(OP_FOR_ITER);

  int exitJump = currentChunk()->count;
  emitByte(0xff);
  emitByte(0xff);

  addLocal(iteratorName);
  markInitialized();

  // --- NEW BLOCK LOGIC ---
  if (match(TOKEN_COLON)) {
    // Block Syntax: for ... : ... end
    TokenType terminators[] = {TOKEN_END};
    block(terminators, 1);

    consume(TOKEN_END, "Expect 'end' after loop.");
  } else {
    // Single Statement Syntax: for ... show i
    statement();
  }
  // -----------------------

  emitByte(OP_POP); // Runtime cleanup of loop var
  emitLoop(loopStart);
  patchJump(exitJump);

  current->localCount--; // Compile-time cleanup
  endScope();
}

static void statement() {
  ignoreNewlines();

  if (match(TOKEN_SHOW)) {
    statementWithGuard(showBody);
  } else if (match(TOKEN_IF)) {
    ifStatement(); // Block, not Guard
  } else if (match(TOKEN_UNLESS)) {
    unlessStatement();
  } else if (match(TOKEN_WHILE)) {
    whileStatement(); // Block, not Guard
  } else if (match(TOKEN_UNTIL)) {
    untilStatement();
  } else if (match(TOKEN_GIVE)) {
    statementWithGuard(returnBody);
  } else if (match(TOKEN_SET)) {
    statementWithGuard(setBody);
  } else if (match(TOKEN_FOR)) {
    forStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    TokenType ends[] = {TOKEN_RIGHT_BRACE};
    block(ends, 1);
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
    consumeEnd(); // Block statements end with newline too
    endScope();
  } else {
    statementWithGuard(expressionBody);
  }
}

ObjFunction *compile(const char *source) {
  initScanner(source);

  // Create the compiler for the main script
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);
  current = &compiler; // Set global pointer

  parser.hadError = false;
  parser.panicMode = false;

  advance();

  // Compile until EOF
  while (!match(TOKEN_EOF)) {
    declaration();
  }

  // Emit return
  emitReturn(); // This writes OP_NIL, OP_RETURN

  // Return the result
  ObjFunction *function = compiler.function;
  if (parser.hadError) {
    // If error, don't return a broken function.
    // (In a real GC language, the function would be collected. Here we just
    // drop it). freeFunction(function); // Todo: Add cleanup logic later
    return NULL;
  }

  return function;
}
