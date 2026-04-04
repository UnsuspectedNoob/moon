#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "error.h"
#include "object.h"
#include "parser.h"
#include "scanner.h"
#include "sigtrie.h"
#include "vm.h"

// ==========================================
// 1. GLOBAL STATE & REGISTRY
// ==========================================

Parser parser;

// --- THE BLINDFOLD STATE ---
typedef struct {
  uint32_t hash;
  int depth;
} ExpectedLabel;

static ExpectedLabel expectedLabelStack[256];
static int expectedLabelCount = 0;
static int groupingDepth = 0; // Tracks if we are inside () or []
static Node *currentStickySubject = NULL;
static int loopingDepth = 0;

// --- ADD THESE TWO LINES ---
static int parseDepth = 0;
#define MAX_AST_DEPTH 256
// ---------------------------

static bool isExpectedLabel() {
  if (expectedLabelCount == 0)
    return false;

  // Hash the current token
  uint32_t currentHash =
      hashString(parser.current.start, parser.current.length);

  // Check if it matches an expected label AT THE CURRENT DEPTH
  for (int i = 0; i < expectedLabelCount; i++) {
    if (expectedLabelStack[i].hash == currentHash &&
        expectedLabelStack[i].depth == groupingDepth) {
      return true;
    }
  }
  return false;
}
// ---------------------------

static char *my_strdup(const char *s) {
  size_t len = strlen(s) + 1;
  char *dup = malloc(len);
  if (dup)
    memcpy(dup, s, len);
  return dup;
}

// --- THE CIRCULAR DEPENDENCY SHIELD ---
#define MAX_LOADED_FILES 256
static char *prepassLoadedFiles[MAX_LOADED_FILES];
static int prepassLoadedCount = 0;

static bool isPrepassLoaded(const char *path) {
  for (int i = 0; i < prepassLoadedCount; i++) {
    if (strcmp(prepassLoadedFiles[i], path) == 0)
      return true;
  }
  return false;
}

static void markPrepassLoaded(const char *path) {
  if (prepassLoadedCount < MAX_LOADED_FILES) {
    prepassLoadedFiles[prepassLoadedCount++] = my_strdup(path);
  }
}
// --------------------------------------

// You will need to expose your readFile helper from main.c or rewrite a quick
// one here
extern char *readFile(const char *path);

static bool canStartExpression(TokenType type) {
  switch (type) {
  case TOKEN_NEWLINE:
  case TOKEN_EOF:
  case TOKEN_RIGHT_PAREN:
  case TOKEN_RIGHT_BRACKET:
  case TOKEN_RIGHT_BRACE:
  case TOKEN_COMMA:
  case TOKEN_COLON:
  case TOKEN_ELSE:
  case TOKEN_BE:
  case TOKEN_TO:
  case TOKEN_IF:
  case TOKEN_UNLESS:
  case TOKEN_THEN:
  case TOKEN_BY:
    return false;
  default:
    return true;
  }
}

bool isMathOperator(Token opToken) {
  switch (opToken.type) {
  case TOKEN_PLUS:
  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
  case TOKEN_MOD:
    return true;
  default:
    return false;
  }
}

void hoistPhrases(const char *source) {
  initScanner(source);
  Token token = scanToken();

  while (token.type != TOKEN_EOF) {
    // --- THE RECURSIVE MODULE JUMP ---
    if (token.type == TOKEN_LOAD) {
      Token pathToken = scanToken();
      if (pathToken.type == TOKEN_STRING) {
        // 1. Extract the file path (chop off quotes)
        char path[1024] = {0};
        strncpy(path, pathToken.start + 1, pathToken.length - 2);

        // 2. Check the Shield!
        if (!isPrepassLoaded(path)) {
          markPrepassLoaded(path);

          // 3. Read the external file
          char *externalSource = readFile(path);
          if (externalSource != NULL) {
            // 4. THE RECURSION: Save the current scanner state!
            Scanner previousScanner = scanner;

            // 5. Dive into the new file and extract its verbs
            hoistPhrases(externalSource);
            free(externalSource);

            // 6. Restore the scanner to pick up where we left off
            scanner = previousScanner;
          }
        }
      }
    }
    // ---------------------------------
    else if (token.type == TOKEN_LET) {
      Token nameToken = scanToken();
      if (nameToken.type == TOKEN_IDENTIFIER) {
        Token next = scanToken();

        if (next.type != TOKEN_BE && next.type != TOKEN_COMMA) {
          char mangled[1024] = {0};
          strncat(mangled, nameToken.start, nameToken.length);

          // Build the root node instantly
          TrieNode *currentNode =
              startPhrase(nameToken.start, nameToken.length);

          if (next.type == TOKEN_COLON) {
            strcat(mangled, "$0");
            finalizePhrase(currentNode, mangled);
          } else {
            while (next.type != TOKEN_COLON && next.type != TOKEN_EOF) {
              if (next.type == TOKEN_NEWLINE) {
                next = scanToken();
                continue;
              }

              if (next.type == TOKEN_LEFT_PAREN) {
                int arity = 0;
                Token peek = scanToken();
                if (peek.type != TOKEN_RIGHT_PAREN) {
                  arity = 1;
                  while (peek.type != TOKEN_RIGHT_PAREN &&
                         peek.type != TOKEN_EOF) {
                    if (peek.type == TOKEN_COMMA)
                      arity++;
                    peek = scanToken();
                  }
                }

                char buf[16];
                sprintf(buf, "$%d", arity);
                strcat(mangled, buf);

                // Add Argument Node
                currentNode = addArgumentBranch(currentNode, arity);
              } else {
                strcat(mangled, "_");
                strncat(mangled, next.start, next.length);

                // Add Label Node
                currentNode =
                    addLabelBranch(currentNode, next.start, next.length);
              }
              next = scanToken();
            }
            finalizePhrase(currentNode, mangled);
          }
        }
      }
    }
    token = scanToken();
  }
  initScanner(source);
}

// ==========================================
// 3. ERROR HANDLING
// ==========================================

void errorAt(Token *token, ErrorType type, const char *message,
             const char *hint) {
  if (parser.panicMode)
    return;
  parser.panicMode = true;
  reportCompileError(token, type, message, hint);
  parser.hadError = true;
}

// Legacy fallback for the Emitter
void error(const char *message) {
  errorAt(&parser.previous, ERR_SYNTAX, message, NULL);
}

void consumeHint(TokenType type, ErrorType errType, const char *message,
                 const char *hint) {
  if (parser.current.type == type) {
    advance();
    return;
  }
  errorAt(&parser.current, errType, message, hint);
}

// --- THE EMPATHETIC BLOCK CLOSER ---
static void consumeBlockEnd(Token opener, const char *blockName) {
  if (check(TOKEN_END)) {
    advance();
    return;
  }

  char message[256];
  snprintf(message, sizeof(message),
           "I couldn't find the 'end' keyword for this %s block.", blockName);

  errorAt(&opener, ERR_SYNTAX, message,
          "Control flow blocks, functions, and types must be closed with the "
          "'end' keyword. "
          "Look at the line I highlighted above to see exactly where this "
          "block started.");
}

// Legacy fallback
void consume(TokenType type, const char *message) {
  consumeHint(type, ERR_SYNTAX, message, NULL);
}

void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_END)
      return;
    switch (parser.current.type) {
    case TOKEN_LET:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_GIVE:
    case TOKEN_SET:
      return;
    default:;
    }
    advance();
  }
}

// ==========================================
// 4. LOW-LEVEL PARSING (Token Control)
// ==========================================

void advance() {
  parser.previous = parser.current;
  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR)
      break;

    errorAt(&parser.current, ERR_SYNTAX, parser.current.start, NULL);
  }
}

bool check(TokenType type) { return parser.current.type == type; }

bool match(TokenType type) {
  if (!check(type))
    return false;

  advance();
  return true;
}

bool checkTerminator(TokenType *terminators, int count) {
  for (int i = 0; i < count; i++) {
    if (check(terminators[i]))
      return true;
  }
  return check(TOKEN_EOF);
}

void ignoreNewlines() {
  while (match(TOKEN_NEWLINE))
    ;
}

// ==========================================
// 5. PRATT ENGINE & EXPRESSIONS
// ==========================================

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,
  PREC_RANGE,
  PREC_OR,
  PREC_AND,
  PREC_EQUALITY,
  PREC_COMPARISON,
  PREC_TERM,
  PREC_FACTOR,
  PREC_CAST,
  PREC_UNARY,
  PREC_CALL,
  PREC_PRIMARY
} Precedence;

typedef Node *(*PrefixFn)();
typedef Node *(*InfixFn)(Node *left);
typedef struct {
  PrefixFn prefix;
  InfixFn infix;
  Precedence precedence;
} ParseRule;

static Node *expression();
static ParseRule *getRule(TokenType type);

static Node *parsePrecedence(Precedence precedence) {
  // 1. Tracks recursive depth (Right-heavy trees)
  parseDepth++;
  if (parseDepth > MAX_AST_DEPTH) {
    error("Expression is too complex or deeply nested. Break it up.");
    parseDepth--;
    return NULL;
  }

  advance();

  PrefixFn prefixRule = getRule(parser.previous.type)->prefix;

  if (prefixRule == NULL) {
    errorAt(&parser.previous, ERR_SYNTAX, "I was expecting an expression here.",
            "An expression evaluates to a value (like a number, a string, or "
            "math). Did you leave a trailing operator?");
    parseDepth--; // Prevent depth leak on error!
    return NULL;
  }

  Node *leftNode = prefixRule();

  // 2. Tracks while-loop depth (Left-heavy trees!)
  int infixDepth = 0;

  while (precedence <= getRule(parser.current.type)->precedence) {
    if (isExpectedLabel())
      break;

    // --- CATCH THE BOMB ---
    infixDepth++;
    if (parseDepth + infixDepth > MAX_AST_DEPTH) {
      error(
          "This expression is too long. Please break it into multiple lines.");
      break;
    }
    // ----------------------

    advance();
    InfixFn infixRule = getRule(parser.previous.type)->infix;
    leftNode = infixRule(leftNode);
  }

  parseDepth--;
  return leftNode;
}

static Node *expression() {
  Node *expr = parsePrecedence(PREC_ASSIGNMENT);

  // Is it a Ternary or a Modifier?
  if (match(TOKEN_IF)) {
    int line = parser.previous.line;
    Node *cond = expression(); // Parse the condition

    if (match(TOKEN_ELSE)) {
      // It has an 'else'! It's a standard Ternary Expression.
      Node *elseBranch = expression();
      return newIfNode(cond, expr, elseBranch, line);
    } else {
      // No 'else'! It's a Statement Modifier. We return it with a NULL
      // elseBranch so the parent statement can invert it.
      return newIfNode(cond, expr, NULL, line);
    }
  } else if (match(TOKEN_UNLESS)) {
    int line = parser.previous.line;
    Node *cond = expression();

    // Invert the condition for 'unless'
    Token notToken = {TOKEN_NOT, "not", 3, line, 0};
    cond = newUnaryNode(notToken, cond, line);

    if (match(TOKEN_THEN)) {
      Node *elseBranch = expression();
      return newIfNode(cond, expr, elseBranch, line);
    } else {
      // Statement Modifier
      return newIfNode(cond, expr, NULL, line);
    }
  }

  return expr;
}

static Node *string() {
  // We chop off the leading and trailing quotes (+1 and -2)
  ObjString *str =
      copyString(parser.previous.start + 1, parser.previous.length - 2);

  return newLiteralNode(OBJ_VAL(str), parser.previous.line);
}

// Helper to chop off the quotes and backticks (+1 and -2)
static Node *extractInterpolationString(Token token) {
  ObjString *str = copyString(token.start + 1, token.length - 2);
  return newLiteralNode(OBJ_VAL(str), token.line);
}

// The Interpolation Parser
static Node *interpolation() {
  int line = parser.previous.line;
  NodeArray parts;
  initNodeArray(&parts);

  // 1. The Opening String ("hello `)
  writeNodeArray(&parts, extractInterpolationString(parser.previous));

  // 2. The Loop
  while (true) {
    // Parse the expression inside the backticks ( x + y )
    writeNodeArray(&parts, expression());

    // It MUST be followed by a MIDDLE or a CLOSE token
    if (match(TOKEN_STRING_MIDDLE)) {
      writeNodeArray(&parts, extractInterpolationString(parser.previous));
    } else if (match(TOKEN_STRING_CLOSE)) {
      writeNodeArray(&parts, extractInterpolationString(parser.previous));
      break;
    } else {
      errorAt(
          &parser.current, ERR_SYNTAX,
          "I was expecting the end of the string interpolation here.",
          "Make sure to close your interpolation block with another backtick "
          "(`) or end the string with a double quote (\")");
      break;
    }
  }

  Node *node = newInterpolationNode(parts.items, parts.count, line);
  freeNodeArray(&parts);
  return node;
}

static Node *number() {
  double value = strtod(parser.previous.start, NULL);
  return newLiteralNode(NUMBER_VAL(value), parser.previous.line);
}

static Node *literal() {
  int line = parser.previous.line;
  switch (parser.previous.type) {
  case TOKEN_FALSE:
    return newLiteralNode(BOOL_VAL(false), line);
  case TOKEN_TRUE:
    return newLiteralNode(BOOL_VAL(true), line);
  case TOKEN_NIL:
    return newLiteralNode(NIL_VAL, line);
  default:
    return NULL;
  }
}

static Node *variable() {
  Token rootToken = parser.previous;

  // --- THE LEXICAL SHIELD ---
  // If the variable starts with '__', it's a VM intrinsic.
  if (rootToken.length >= 2 && rootToken.start[0] == '_' &&
      rootToken.start[1] == '_') {
    // If the core is already done booting, the user is typing this! Reject it.
    if (isCoreBootstrapped) {
      errorAt(&parser.previous, ERR_SYNTAX,
              "You may not use identifiers starting with '__'.",
              "This prefix is strictly reserved for internal MOON engine "
              "primitives.. so piss off..");
      return newVariableNode(rootToken,
                             rootToken.line); // Return safe dummy node
    }
  }
  // --------------------------

  char rootWord[256] = {0};
  snprintf(rootWord, rootToken.length + 1, "%.*s", rootToken.length,
           rootToken.start);

  TrieNode *currentNode = getSignatureTrie(rootWord);

  if (currentNode == NULL) {
    return newVariableNode(rootToken, rootToken.line);
  }

  // Use dynamic array for accumulated arguments
  NodeArray args;
  initNodeArray(&args);

  TrieNode *startNode = currentNode;
  TrieNode *lastGoodState = currentNode->isTerminal ? currentNode : NULL;

  while (currentNode->childCount > 0) {
    uint32_t nextHash = hashString(parser.current.start, parser.current.length);
    TrieNode *matchedLabel = NULL;
    bool expectsArgument = false;

    for (int i = 0; i < currentNode->childCount; i++) {
      TrieNode *child = currentNode->children[i];
      if (child->type == NODE_LABEL && child->labelHash == nextHash) {
        matchedLabel = child;
      } else if (child->type == NODE_ARGUMENT) {
        expectsArgument = true;
      }
    }

    if (matchedLabel != NULL) {
      advance();
      currentNode = matchedLabel;
      if (currentNode->isTerminal)
        lastGoodState = currentNode;
      continue;
    }

    if (expectsArgument && canStartExpression(parser.current.type)) {
      int labelsPushed = 0;
      for (int i = 0; i < currentNode->childCount; i++) {
        if (currentNode->children[i]->type == NODE_ARGUMENT) {
          TrieNode *ac = currentNode->children[i];
          for (int j = 0; j < ac->childCount; j++) {
            if (ac->children[j]->type == NODE_LABEL) {
              expectedLabelStack[expectedLabelCount].hash =
                  ac->children[j]->labelHash;
              expectedLabelStack[expectedLabelCount].depth = groupingDepth;
              expectedLabelCount++;
              labelsPushed++;
            }
          }
        }
      }

      // Initialize temp array INSIDE the loop
      NodeArray tempArgs;
      initNodeArray(&tempArgs);

      if (check(TOKEN_LEFT_PAREN)) {
        advance();
        groupingDepth++;

        if (!check(TOKEN_RIGHT_PAREN)) {
          do {
            writeNodeArray(&tempArgs, expression());
          } while (match(TOKEN_COMMA));
        }
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
        groupingDepth--;
      } else {
        writeNodeArray(&tempArgs, expression());
      }

      expectedLabelCount -= labelsPushed;

      TrieNode *matchedArgChild = NULL;
      for (int i = 0; i < currentNode->childCount; i++) {
        if (currentNode->children[i]->type == NODE_ARGUMENT &&
            currentNode->children[i]->arity == tempArgs.count) {
          matchedArgChild = currentNode->children[i];
          break;
        }
      }

      if (matchedArgChild != NULL) {
        // Transfer contents
        for (int i = 0; i < tempArgs.count; i++)
          writeNodeArray(&args, tempArgs.items[i]);

        freeNodeArray(&tempArgs); // Free the temp buffer before looping!

        currentNode = matchedArgChild;
        if (currentNode->isTerminal)
          lastGoodState = currentNode;
        continue;
      } else {
        freeNodeArray(&tempArgs); // Free on dead end!
        break;
      }
    }
    break;
  }

  if (lastGoodState != NULL) {
    if (currentNode != lastGoodState) {
      errorAt(&parser.previous, ERR_SYNTAX,
              "This phrasal function looks incomplete.",
              "You started a phrase but didn't finish it. Check the function's "
              "signature to see what words or arguments are missing.");
      freeNodeArray(&args);
      return newVariableNode(rootToken, rootToken.line);
    }

    Token mangledToken = rootToken;
    mangledToken.start = my_strdup(lastGoodState->mangledName);
    mangledToken.length = strlen(lastGoodState->mangledName);

    Node *node = newPhrasalCallNode(mangledToken, args.items, args.count,
                                    rootToken.line);
    freeNodeArray(&args);
    return node;
  }

  if (currentNode != startNode) {
    errorAt(
        &parser.previous, ERR_REFERENCE,
        "I don't recognize this phrasal function.",
        "Did you misspell a word, or forget to define this function earlier?");
  }

  freeNodeArray(&args);
  return newVariableNode(rootToken, rootToken.line);
}

static Node *explicitSticky() {
  Node *right = parsePrecedence(PREC_UNARY);
  currentStickySubject = right; // Lock it into the register!
  return right; // Act transparently, passing the value right through
}

static Node *unary() {
  Token opToken = parser.previous;
  Node *right = parsePrecedence(PREC_UNARY);
  return newUnaryNode(opToken, right, opToken.line);
}

// Safely clones an AST node so we don't double-free memory!
static Node *cloneNode(Node *original) {
  if (!original)
    return NULL;

  switch (original->type) {
  case NODE_VARIABLE:
    return newVariableNode(original->as.variable.name, original->line);
  case NODE_LITERAL:
    return newLiteralNode(original->as.literal.value, original->line);
  case NODE_PROPERTY:
    return newPropertyNode(cloneNode(original->as.property.target),
                           original->as.property.name, original->line);
  case NODE_SUBSCRIPT:
    return newSubscriptNode(cloneNode(original->as.subscript.left),
                            cloneNode(original->as.subscript.index),
                            original->line);
  default:
    errorAt(&parser.previous, ERR_SYNTAX,
            "I can't use a complex expression as a sticky subject.",
            "A sticky subject must be a simple variable, property, or "
            "subscript (e.g., 'player' or 'player's health').");
    return NULL;
  }
}

static Node *stickyPrefix() {
  if (currentStickySubject == NULL) {
    errorAt(&parser.previous, ERR_SYNTAX,
            "This operator is missing its left side.",
            "It looks like you used an operator (like '<', '>=', or 'is') "
            "without providing a value on its left, and there is no active "
            "subject to attach it to.");
    return NULL;
  }

  Token opToken = parser.previous;

  // --- THE "IS NOT" FIX ---
  bool invert = false;
  if ((opToken.type == TOKEN_IS) && match(TOKEN_NOT)) {
    invert = true;
  }

  ParseRule *rule = getRule(opToken.type);
  Node *right = parsePrecedence((Precedence)(rule->precedence + 1));

  // Build the binary node using the cloned subject!
  Node *stickyNode = newBinaryNode(cloneNode(currentStickySubject), opToken,
                                   right, opToken.line);

  if (invert) {
    Token notToken = {TOKEN_NOT, "not", 3, opToken.line, 0};
    return newUnaryNode(notToken, stickyNode, opToken.line);
  }

  return stickyNode;
}

static bool isComparison(Token opToken) {

  switch (opToken.type) {
  case TOKEN_IS:
  case TOKEN_EQUAL_EQUAL:
  case TOKEN_LESS_EQUAL:
  case TOKEN_LESS:
  case TOKEN_GREATER_EQUAL:
  case TOKEN_GREATER:
  case TOKEN_EQUAL:
    return true;

  default:
    return false;
  }
}

static Node *castExpression(Node *left) {
  int line = parser.previous.line;
  // Parse the right side (the Target Type) with exact CAST precedence!
  Node *right = parsePrecedence(PREC_CAST);
  return newCastNode(left, right, line);
}

static Node *binary(Node *left) {
  Token opToken = parser.previous;

  // --- STICKY SUBJECT CAPTURE ---
  if (isComparison(opToken)) {
    currentStickySubject = left;
  }

  // --- THE "IS NOT" FIX ---
  bool invert = false;
  if ((opToken.type == TOKEN_IS) && match(TOKEN_NOT)) {
    invert = true;
  }

  ParseRule *rule = getRule(opToken.type);
  Node *right = parsePrecedence((Precedence)(rule->precedence + 1));
  Node *binNode = newBinaryNode(left, opToken, right, opToken.line);

  // If we caught a 'not', wrap the whole binary expression!
  if (invert) {
    Token notToken = {TOKEN_NOT, "not", 3, opToken.line, 0};
    return newUnaryNode(notToken, binNode, opToken.line);
  }

  return binNode;
}

static Node *and_(Node *left) {
  Token opToken = parser.previous;
  Node *right = parsePrecedence(PREC_AND);
  return newLogicalNode(left, opToken, right, opToken.line);
}

static Node *or_(Node *left) {
  Token opToken = parser.previous;
  Node *right = parsePrecedence(PREC_OR);
  return newLogicalNode(left, opToken, right, opToken.line);
}

static Node *list() {
  int line = parser.previous.line;
  NodeArray itemsArr, *items = &itemsArr;
  initNodeArray(items);

  groupingDepth++; // Protect the list items!
  if (!check(TOKEN_RIGHT_BRACKET)) {
    do {
      ignoreNewlines();
      if (check(TOKEN_RIGHT_BRACKET))
        break;
      writeNodeArray(items, expression());
    } while (match(TOKEN_COMMA));
  }
  groupingDepth--; // Coming back out!

  ignoreNewlines();
  consumeHint(TOKEN_RIGHT_BRACKET, ERR_SYNTAX,
              "I couldn't find the closing bracket ']' for this list.",
              "Make sure your list ends with ']' and that all items inside are "
              "separated by commas.");

  Node *node = newListNode(items->items, items->count, line);
  freeNodeArray(items);
  return node;
}

static Node *dict() {
  int line = parser.previous.line;
  NodeArray keys;
  initNodeArray(&keys);

  NodeArray values;
  initNodeArray(&values);

  groupingDepth++; // Protect from blindfolds!

  if (!check(TOKEN_RIGHT_BRACE)) {
    do {
      ignoreNewlines();
      if (check(TOKEN_RIGHT_BRACE))
        break;

      // 1. Parse the Key (Force it to be a String Literal!)
      Node *keyNode = NULL;
      if (match(TOKEN_IDENTIFIER)) {
        // If they type `name:`, convert the bare identifier into the string
        // "name"
        ObjString *keyStr =
            copyString(parser.previous.start, parser.previous.length);
        keyNode = newLiteralNode(OBJ_VAL(keyStr), parser.previous.line);
      } else if (match(TOKEN_STRING)) {
        // If they type `"name":`, chop off the quotes normally
        ObjString *keyStr =
            copyString(parser.previous.start + 1, parser.previous.length - 2);
        keyNode = newLiteralNode(OBJ_VAL(keyStr), parser.previous.line);
      } else {
        errorAt(&parser.previous, ERR_SYNTAX,
                "I was expecting a property name for this dictionary item.",
                "Dictionary keys should be words or strings (e.g., 'name:' "
                "or '\"age\":').");
        break;
      }

      // 2. The Colon
      consumeHint(TOKEN_COLON, ERR_SYNTAX, "I was expecting a colon ':' here.",
                  "In dictionaries and blueprints, properties must be followed "
                  "by a colon (e.g., 'name: \"Munachi\"').");

      // 3. The Value
      Node *valueNode = expression();

      writeNodeArray(&keys, keyNode);
      writeNodeArray(&values, valueNode);
    } while (match(TOKEN_COMMA));
  }

  groupingDepth--; // Coming back out!

  ignoreNewlines();
  consumeHint(TOKEN_RIGHT_BRACE, ERR_SYNTAX,
              "I couldn't find the closing brace '}' for this dictionary.",
              "Make sure your dictionary ends with '}'.");

  Node *node = newDictNode(keys.items, values.items, keys.count, line);
  freeNodeArray(&keys);
  freeNodeArray(&values);
  return node;
}

static Node *parseInstantiate(Node *left, bool isWith) {
  int line = parser.previous.line;
  Token blockOpener = parser.previous;

  TokenArray propNames;
  initTokenArray(&propNames);
  NodeArray values;
  initNodeArray(&values);

  // Dynamically swap the terminator!
  TokenType terminator = isWith ? TOKEN_END : TOKEN_RIGHT_BRACE;

  if (!isWith)
    groupingDepth++; // Protect from blindfolds only for {}

  if (!check(terminator)) {
    do {
      ignoreNewlines();
      if (check(terminator))
        break;

      errorAt(
          &parser.current, ERR_SYNTAX, "I was expecting a property name here.",
          isWith
              ? "When overriding properties, you need to list them explicitly."
              : "When instantiating a type, you need to list its properties "
                "(e.g., 'health: 100').");
      if (parser.current.type == TOKEN_IDENTIFIER)
        advance(); // Eat the identifier

      writeTokenArray(&propNames, parser.previous);

      consumeHint(
          TOKEN_COLON, ERR_SYNTAX, "I was expecting a colon ':' here.",
          "Provide a colon after the property name to assign its value.");

      writeNodeArray(&values, expression());
    } while (match(TOKEN_COMMA));
  }

  if (!isWith)
    groupingDepth--;

  ignoreNewlines();

  if (isWith) {
    consumeBlockEnd(blockOpener, "'with' override");
  } else {
    consumeHint(TOKEN_RIGHT_BRACE, ERR_SYNTAX,
                "I couldn't find the closing brace '}' for this instance.",
                "Make sure your instance block ends with '}'.");
  }

  Node *node = newInstantiateNode(left, propNames.items, values.items,
                                  propNames.count, line);
  freeTokenArray(&propNames);
  freeNodeArray(&values);
  return node;
}

// The tiny 1-line wrappers for the Pratt Rule Table!
static Node *instantiate(Node *left) { return parseInstantiate(left, false); }
static Node *instantiateWith(Node *left) {
  return parseInstantiate(left, true);
}

static Node *subscript(Node *left) {
  int line = parser.previous.line;

  groupingDepth++; // Protect the index!
  Node *index = expression();
  groupingDepth--; // Coming back out!

  consumeHint(TOKEN_RIGHT_BRACKET, ERR_SYNTAX,
              "I couldn't find the closing bracket ']' for this index.",
              "When accessing an item by its index, close the brackets (e.g., "
              "'list[1]').");
  return newSubscriptNode(left, index, line);
}

static Node *dot(Node *left) {
  int line = parser.previous.line;

  Node *index = parsePrecedence((Precedence)(PREC_CALL + 1));

  return newSubscriptNode(left, index, line);
}

static Node *range(Node *left) {
  int line = parser.previous.line;

  // The 'left' node is the start of the range (e.g., 1)
  // We parse the 'end' with PREC_RANGE + 1 so it binds tightly
  Node *endNode = parsePrecedence((Precedence)(PREC_RANGE + 1));

  Node *stepNode = NULL;
  if (match(TOKEN_BY)) {
    // If there is a 'by', parse the step size
    stepNode = parsePrecedence((Precedence)(PREC_RANGE + 1));
  } else {
    // Default step is 1.0
    stepNode = newLiteralNode(NUMBER_VAL(1.0), line);
  }

  return newRangeNode(left, endNode, stepNode, line);
}

static Node *possessive(Node *left) {
  int line = parser.previous.line;

  // Moon's Property Accessor!
  // e.g. `player's health`
  consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
              "I was expecting a property name after the possessive 's.",
              "Provide the name of the property you want to access (e.g., "
              "'user's age').");
  Token name = parser.previous;

  return newPropertyNode(left, name, line);
}

static Node *endKeyword() { return newEndNode(parser.previous.line); }

// ==========================================
// 6. STATEMENTS & DECLARATIONS
// ==========================================

static Node *declaration();
static Node *statement();

static Node *block(TokenType *terminators, int count) {
  int line = parser.previous.line;
  NodeArray statements;
  initNodeArray(&statements);

  while (true) {
    ignoreNewlines();
    if (checkTerminator(terminators, count) || check(TOKEN_EOF))
      break;

    writeNodeArray(&statements, declaration());
  }
  Node *node = newBlockNode(statements.items, statements.count, line);
  freeNodeArray(&statements);
  return node;
}

static Node *ifStatement(bool invert) {
  int line = parser.previous.line;
  Node *condition = expression();

  if (invert) {
    Token notToken = {TOKEN_NOT, "not", 3, line, 0};
    condition = newUnaryNode(notToken, condition, line);
  }

  Node *thenBranch = NULL, *elseBranch = NULL;

  // THE KEYWORD SWAP: 'if' uses 'else', 'unless' uses 'then'
  TokenType altToken = invert ? TOKEN_THEN : TOKEN_ELSE;

  if (match(TOKEN_COLON)) {
    Token thenStart = parser.previous; // <- capture the colon
    TokenType thenEnds[] = {altToken, TOKEN_END};
    thenBranch = block(thenEnds, 2);

    if (match(altToken)) {
      Token elseStart = parser.previous; // <- capture the 'else' / 'then'
      if (match(TOKEN_IF)) {
        elseBranch = ifStatement(false);
      } else if (match(TOKEN_UNLESS)) {
        elseBranch = ifStatement(true);
      } else if (match(TOKEN_COLON)) {
        TokenType elseEnds[] = {TOKEN_END};
        elseBranch = block(elseEnds, 1);
        consumeBlockEnd(elseStart, "alternate");
      } else {
        elseBranch = statement();
      }
    } else {
      consumeBlockEnd(thenStart, "if/unless");
    }
  } else {
    // Single-statement branches
    thenBranch = statement();

    ignoreNewlines(); // Skip line breaks before checking for the alternate
                      // branch!

    if (match(altToken)) {
      if (match(TOKEN_IF))
        elseBranch = ifStatement(false);
      else if (match(TOKEN_UNLESS))
        elseBranch = ifStatement(true);
      else
        elseBranch = statement();
    }
  }

  return newIfNode(condition, thenBranch, elseBranch, line);
}

static Node *whileLogic(bool invert) {
  int line = parser.previous.line;
  Node *condition = expression();

  if (invert) {
    Token notToken = {TOKEN_NOT, "not", 3, line, 0};
    condition = newUnaryNode(notToken, condition, line);
  }

  Node *body = NULL;
  loopingDepth++;

  if (match(TOKEN_COLON)) {
    Token loopStart = parser.previous; // <- capture the colon
    TokenType terminators[] = {TOKEN_END};
    body = block(terminators, 1);
    consumeBlockEnd(loopStart, "while/until loop");
  } else {
    body = statement();
  }

  loopingDepth--;

  return newWhileNode(condition, body, line);
}

static Node *forStatement() {
  int line = parser.previous.line;
  if (check(TOKEN_EACH))
    advance();

  Token iteratorName = parser.current;
  consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
              "I was expecting a variable name here.",
              "Provide a valid name for your loop iterator (e.g., 'for each "
              "item in list:').");

  if (check(TOKEN_IN))
    advance();
  else if (check(TOKEN_FROM))
    advance();

  Node *sequence = expression();
  Node *body = NULL;

  loopingDepth++;
  if (match(TOKEN_COLON)) {
    Token loopStart = parser.previous;
    TokenType terminators[] = {TOKEN_END};
    body = block(terminators, 1);
    consumeBlockEnd(loopStart, "for loop");
  } else {
    body = statement();
  }
  loopingDepth--;

  return newForNode(iteratorName, sequence, body, line);
}

static Node *parseLValue() {
  // Base Case: The root must be an identifier
  consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
              "I was expecting a variable name to assign to.",
              "Assignments need a target, like 'set score to 10'.");

  Node *lvalue = newVariableNode(parser.previous, parser.previous.line);

  // The Recursive Modifier Loop: [ <expr> ] | . <expr> | 's <id>
  while (true) {
    if (match(TOKEN_LEFT_BRACKET)) {
      lvalue = subscript(lvalue);
    } else if (match(TOKEN_DOT)) {
      lvalue = dot(lvalue);
    } else if (match(TOKEN_POSSESSIVE)) {
      lvalue = possessive(lvalue);
    } else {
      break; // No more modifiers, the memory address is fully resolved!
    }
  }

  return lvalue;
}

static Node *addStatement() {
  int line = parser.previous.line;
  NodeArray values;
  initNodeArray(&values);

  // --- THE BLINDFOLD FIX ---
  // We manually push "to" onto the expected label stack so the Pratt parser
  // knows to stop when it sees it, rather than eating it as a range operator!
  expectedLabelStack[expectedLabelCount].hash = hashString("to", 2);
  expectedLabelStack[expectedLabelCount].depth = groupingDepth;
  expectedLabelCount++;

  // 1. Gather all the expressions to add
  do {
    writeNodeArray(&values, expression());
  } while (match(TOKEN_COMMA));

  // Pop the blindfold!
  expectedLabelCount--;
  // -------------------------

  // 2. The Bridge
  if (match(TOKEN_PLUS)) {
    errorAt(&parser.previous, ERR_SYNTAX,
            "It looks like you used '+' inside an 'add' statement.",
            "The 'add' phrase already implies addition! Use the word 'to' "
            "instead (e.g., 'add 5 to score').");
  } else {
    consumeHint(
        TOKEN_TO, ERR_SYNTAX, "I was expecting the 'to' keyword here.",
        "The 'add' statement is formatted as 'add <value> to <target>'.");
  }

  // 3. The Target (L-Value)
  Node *target = parseLValue();

  // 4. THE DESUGARING (Build the Accumulator)
  Node *accumulator = cloneNode(target);
  if (accumulator == NULL) {
    errorAt(&parser.previous, ERR_SYNTAX,
            "This isn't a valid target for 'add'.",
            "You can only add to variables, subscripts, or properties (e.g., "
            "'add 5 to player's score').");
    return NULL;
  }

  Token plusToken = {
      .type = TOKEN_ADD_INPLACE, .start = "add", .length = 3, .line = line};

  for (int i = 0; i < values.count; i++) {
    accumulator = newBinaryNode(accumulator, plusToken, values.items[i], line);
  }

  freeNodeArray(&values);

  // 5. Wrap it all in a SET node
  Node *targets[1] = {target};
  Node *setValues[1] = {accumulator};

  Node *addNode = newSetNode(targets, 1, setValues, 1, line);

  // --- OPTIONAL: AST INVERSION (Statement Modifiers) ---
  if (match(TOKEN_IF)) {
    Node *cond = expression();
    return newIfNode(cond, addNode, NULL, line);
  } else if (match(TOKEN_UNLESS)) {
    Node *cond = expression();
    Token notToken = {TOKEN_NOT, "not", 3, line, 0};
    Node *invertedCond = newUnaryNode(notToken, cond, line);
    return newIfNode(invertedCond, addNode, NULL, line);
  }

  return addNode;
}

static Node *setStatement() {
  int line = parser.previous.line;
  NodeArray targets;
  initNodeArray(&targets);
  NodeArray values;
  initNodeArray(&values);

  do {
    writeNodeArray(&targets, parseLValue());
  } while (match(TOKEN_COMMA));

  // --- FOREIGN SYNTAX INTERCEPT: '=' instead of 'to' ---
  if (match(TOKEN_EQUAL)) {
    errorAt(&parser.previous, ERR_SYNTAX,
            "It looks like you used '=' to update a variable.",
            "In MOON, we use the word 'to' for updates. Try changing '=' to "
            "'to' (e.g., 'set score to 100').");
  } else {
    // If it wasn't an '=', fall back to the standard strict check
    consumeHint(
        TOKEN_TO, ERR_SYNTAX, "I was expecting the 'to' keyword here.",
        "The 'set' statement is formatted as 'set <target> to <value>'.");
  }

  do {
    writeNodeArray(&values, expression());
  } while (match(TOKEN_COMMA));

  Node *lastVal = values.items[values.count - 1];
  if (lastVal != NULL && lastVal->type == NODE_IF &&
      lastVal->as.ifStmt.elseBranch == NULL) {
    Node *cond = lastVal->as.ifStmt.condition;
    values.items[values.count - 1] = lastVal->as.ifStmt.thenBranch;

    Node *setNode = newSetNode(targets.items, targets.count, values.items,
                               values.count, line);

    freeNodeArray(&targets);
    freeNodeArray(&values); // FREE BEFORE RETURN!
    return newIfNode(cond, setNode, NULL, line);
  }

  if (values.count > 1 && values.count != targets.count) {
    error("Mismatch in assignment counts.");
  }

  Node *node = newSetNode(targets.items, targets.count, values.items,
                          values.count, line);
  freeNodeArray(&targets);
  freeNodeArray(&values);
  return node;
}

static Node *giveStatement() {
  int line = parser.previous.line;
  Node *expr = NULL;

  if (!check(TOKEN_NEWLINE) && !check(TOKEN_EOF) && !check(TOKEN_END)) {
    expr = expression();
  }

  // THE AST INVERSION
  if (expr != NULL && expr->type == NODE_IF &&
      expr->as.ifStmt.elseBranch == NULL) {
    Node *cond = expr->as.ifStmt.condition;
    Node *inner = expr->as.ifStmt.thenBranch;
    Node *giveStmt = newSingleExprNode(NODE_RETURN, inner, line);
    return newIfNode(cond, giveStmt, NULL, line); // Wrap the give!
  }

  return newSingleExprNode(NODE_RETURN, expr, line);
}

static Node *expressionStatement() {
  int line = parser.current.line;
  Node *expr = expression();

  // --- THE BUBBLE-UP FIX ---
  // If the expression was a Phrasal Call, check if its last argument absorbed
  // the modifier!
  if (expr != NULL && expr->type == NODE_PHRASAL_CALL) {
    int lastIdx = expr->as.phrasalCall.argCount - 1;
    if (lastIdx >= 0) {
      Node *lastArg = expr->as.phrasalCall.arguments[lastIdx];
      if (lastArg->type == NODE_IF && lastArg->as.ifStmt.elseBranch == NULL) {
        Node *cond = lastArg->as.ifStmt.condition;
        // Put the raw string back into the argument slot
        expr->as.phrasalCall.arguments[lastIdx] = lastArg->as.ifStmt.thenBranch;
        // Wrap the entire call in the If block
        Node *exprStmt = newSingleExprNode(NODE_EXPRESSION_STMT, expr, line);
        return newIfNode(cond, exprStmt, NULL, line);
      }
    }
  }

  // Do the exact same check for standard function calls!
  if (expr != NULL && expr->type == NODE_CALL) {
    int lastIdx = expr->as.call.argCount - 1;
    if (lastIdx >= 0) {
      Node *lastArg = expr->as.call.arguments[lastIdx];
      if (lastArg->type == NODE_IF && lastArg->as.ifStmt.elseBranch == NULL) {
        Node *cond = lastArg->as.ifStmt.condition;
        expr->as.call.arguments[lastIdx] = lastArg->as.ifStmt.thenBranch;
        Node *exprStmt = newSingleExprNode(NODE_EXPRESSION_STMT, expr, line);
        return newIfNode(cond, exprStmt, NULL, line);
      }
    }
  }

  // The standard AST Inversion (for normal assignments/math)
  if (expr != NULL && expr->type == NODE_IF &&
      expr->as.ifStmt.elseBranch == NULL) {
    Node *cond = expr->as.ifStmt.condition;
    Node *inner = expr->as.ifStmt.thenBranch;
    Node *exprStmt = newSingleExprNode(NODE_EXPRESSION_STMT, inner, line);
    return newIfNode(cond, exprStmt, NULL, line);
  }

  return newSingleExprNode(NODE_EXPRESSION_STMT, expr, line);
}

static Node *breakStatement() {
  if (loopingDepth == 0) {
    errorAt(&parser.previous, ERR_SYNTAX,
            "I found a 'break' or 'quit' outside of a loop.",
            "These keywords can only be used inside 'while' or 'for' loops "
            "to exit them early.");
  }
  return newBreakNode(parser.previous.line);
}

static Node *skipStatement() {
  if (loopingDepth == 0) {
    errorAt(&parser.previous, ERR_SYNTAX, "I found a 'skip' outside of a loop.",
            "The 'skip' keyword can only be used inside loops to jump to the "
            "next iteration.");
  }
  return newSkipNode(parser.previous.line);
}

static Node *typeDeclaration() {
  int line = parser.previous.line;
  consumeHint(
      TOKEN_IDENTIFIER, ERR_SYNTAX, "I was expecting a name for this type.",
      "When defining a type, you must give it a name (e.g., 'type Player:').");
  Token name = parser.previous;
  consumeHint(TOKEN_COLON, ERR_SYNTAX,
              "I was expecting a colon ':' after the type name.",
              "Type definitions begin with a colon before listing properties.");

  Token typeStart = parser.previous;

  TokenArray propertyNames;
  initTokenArray(&propertyNames);
  NodeArray defaultValues;
  initNodeArray(&defaultValues);

  ignoreNewlines();

  if (!check(TOKEN_END)) {
    do {
      ignoreNewlines();
      if (check(TOKEN_END))
        break; // Allow trailing commas before 'end'

      consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
                  "I was expecting a property name here.",
                  "List the properties that belong to this type.");
      writeTokenArray(&propertyNames, parser.previous);

      // If they type `be 300`, parse the 300. Otherwise, silently default to
      // `nil`.
      if (match(TOKEN_COLON)) {
        writeNodeArray(&defaultValues, expression());
      } else {
        writeNodeArray(&defaultValues,
                       newLiteralNode(NIL_VAL, parser.previous.line));
      }
    } while (match(TOKEN_COMMA));
  }

  ignoreNewlines();
  consumeBlockEnd(typeStart, "type definition");

  Node *node = newTypeNode(name, propertyNames.items, defaultValues.items,
                           propertyNames.count, line);
  freeTokenArray(&propertyNames);
  freeNodeArray(&defaultValues);
  return node;
}

// Helper to create our invisible ghost tokens
static Token makeHiddenToken(const char *text, int line) {
  Token t;
  t.type = TOKEN_IDENTIFIER;
  t.start = text;
  t.length = strlen(text);
  t.line = line;
  t.column = 0;
  return t;
}

static Node *updateStatement() {
  int line = parser.previous.line;

  // 1. The Target (L-Value)
  Node *rawTarget = parseLValue();

  // 2. The Operator Fork!
  bool isCastUpdate = false;
  Node *castType = NULL;
  Token opToken;
  Node *mathValue = NULL;
  Node *modifierCond = NULL;

  if (match(TOKEN_AS)) {
    isCastUpdate = true;
    castType = parsePrecedence(PREC_CAST);
  } else if (isMathOperator(parser.current)) {
    advance();
    opToken = parser.previous;
    if (opToken.type == TOKEN_PLUS)
      opToken.type = TOKEN_ADD_INPLACE;
    mathValue = expression();
    // --- THE MODIFIER UNWRAP FIX ---
    // If expression() accidentally ate a statement modifier, unwrap it!
    if (mathValue != NULL && mathValue->type == NODE_IF &&
        mathValue->as.ifStmt.elseBranch == NULL) {
      modifierCond = mathValue->as.ifStmt.condition;
      mathValue = mathValue->as.ifStmt.thenBranch;
    }
  } else {
    errorAt(&parser.current, ERR_SYNTAX,
            "I was expecting 'as' or a math operator here.",
            "The update statement requires 'as' (to cast) or a math operator "
            "(+, -, *, /, %). e.g., 'update x as List' or 'update x * 2'.");
    return NULL;
  }

  // 3. THE DESUGARING (Building the Universal RHS)
  Node *finalNode = NULL;

  if (rawTarget->type == NODE_VARIABLE) {
    Node *accumulator = cloneNode(rawTarget);
    Node *rhsNode = isCastUpdate
                        ? newCastNode(accumulator, castType, line)
                        : newBinaryNode(accumulator, opToken, mathValue, line);

    Node *targets[1] = {rawTarget};
    Node *setValues[1] = {rhsNode};
    finalNode = newSetNode(targets, 1, setValues, 1, line);

  } else if (rawTarget->type == NODE_PROPERTY) {
    Token objToken = makeHiddenToken(" obj", line);
    Token nArr[1] = {objToken};
    Node *eArr[1] = {rawTarget->as.property.target};
    Node *letObj = newLetNode(nArr, 1, eArr, 1, line);

    Node *safeObj1 = newVariableNode(objToken, line);
    Node *safeTarget1 =
        newPropertyNode(safeObj1, rawTarget->as.property.name, line);

    Node *safeObj2 = newVariableNode(objToken, line);
    Node *rhsReader =
        newPropertyNode(safeObj2, rawTarget->as.property.name, line);

    // Apply the fork!
    Node *rhsNode = isCastUpdate
                        ? newCastNode(rhsReader, castType, line)
                        : newBinaryNode(rhsReader, opToken, mathValue, line);

    Node *targets[1] = {safeTarget1};
    Node *setValues[1] = {rhsNode};
    Node *setStmt = newSetNode(targets, 1, setValues, 1, line);

    Node *blockStmts[2] = {letObj, setStmt};
    finalNode = newBlockNode(blockStmts, 2, line);

    FREE(Node, rawTarget);

  } else if (rawTarget->type == NODE_SUBSCRIPT) {
    Token objToken = makeHiddenToken(" obj", line);
    Token nArr1[1] = {objToken};
    Node *eArr1[1] = {rawTarget->as.subscript.left};
    Node *letObj = newLetNode(nArr1, 1, eArr1, 1, line);

    Token idxToken = makeHiddenToken(" idx", line);
    Token nArr2[1] = {idxToken};
    Node *eArr2[1] = {rawTarget->as.subscript.index};
    Node *letIdx = newLetNode(nArr2, 1, eArr2, 1, line);

    Node *safeObj1 = newVariableNode(objToken, line);
    Node *safeIdx1 = newVariableNode(idxToken, line);
    Node *safeTarget1 = newSubscriptNode(safeObj1, safeIdx1, line);

    Node *safeObj2 = newVariableNode(objToken, line);
    Node *safeIdx2 = newVariableNode(idxToken, line);
    Node *rhsReader = newSubscriptNode(safeObj2, safeIdx2, line);

    // Apply the fork!
    Node *rhsNode = isCastUpdate
                        ? newCastNode(rhsReader, castType, line)
                        : newBinaryNode(rhsReader, opToken, mathValue, line);

    Node *targets[1] = {safeTarget1};
    Node *setValues[1] = {rhsNode};
    Node *setStmt = newSetNode(targets, 1, setValues, 1, line);

    Node *blockStmts[3] = {letObj, letIdx, setStmt};
    finalNode = newBlockNode(blockStmts, 3, line);

    FREE(Node, rawTarget);
  }

  // --- 4. RE-WRAP THE AST ---
  // If we unwrapped a stolen modifier earlier, apply it to the whole statement
  // now!
  if (modifierCond != NULL) {
    return newIfNode(modifierCond, finalNode, NULL, line);
  }

  // --- OPTIONAL: AST INVERSION (Statement Modifiers) ---
  if (match(TOKEN_IF)) {
    Node *cond = expression();
    return newIfNode(cond, finalNode, NULL, line);
  } else if (match(TOKEN_UNLESS)) {
    Node *cond = expression();
    Token notToken = {TOKEN_NOT, "not", 3, line, 0};
    Node *invertedCond = newUnaryNode(notToken, cond, line);
    return newIfNode(invertedCond, finalNode, NULL, line);
  }

  return finalNode;
}

static Node *loadStatement() {
  int line = parser.previous.line;
  consumeHint(TOKEN_STRING, ERR_SYNTAX, "I was expecting a file path here.",
              "The load statement requires a file path in quotes (e.g., load "
              "\"math.moon\").");
  return newLoadNode(parser.previous, line);
}

static Node *statement() {
  currentStickySubject = NULL;
  ignoreNewlines();

  // --- 1. Block Statements (Control Flow) ---
  if (match(TOKEN_IF))
    return ifStatement(false);
  if (match(TOKEN_UNLESS))
    return ifStatement(true);
  if (match(TOKEN_WHILE))
    return whileLogic(false);
  if (match(TOKEN_UNTIL))
    return whileLogic(true);
  if (match(TOKEN_FOR))
    return forStatement();

  // --- 2. Action Statements ---
  Node *stmt = NULL;

  if (match(TOKEN_BREAK) || match(TOKEN_QUIT)) {
    stmt = breakStatement();
  } else if (match(TOKEN_SKIP)) {
    stmt = skipStatement();
  } else if (match(TOKEN_LOAD)) {
    stmt = loadStatement();
  } else if (match(TOKEN_SET)) {
    stmt = setStatement();
  } else if (match(TOKEN_ADD)) {
    stmt = addStatement();
  } else if (match(TOKEN_GIVE)) {
    stmt = giveStatement();
  } else if (match(TOKEN_UPDATE)) {
    stmt = updateStatement();
  } else {
    stmt = expressionStatement();
  }

  return stmt;
}

static Node *letDeclaration() {
  TokenArray names;
  initTokenArray(&names);

  do {
    // THE UX UPGRADE
    consumeHint(
        TOKEN_IDENTIFIER, ERR_SYNTAX, "I was expecting a variable name here.",
        "Variables need a name to identify them. e.g., 'let count be 10'");

    writeTokenArray(&names, parser.previous);
  } while (match(TOKEN_COMMA));

  if (match(TOKEN_BE)) {
    NodeArray exprs;
    initNodeArray(&exprs);

    do {
      writeNodeArray(&exprs, expression());
    } while (match(TOKEN_COMMA));

    Node *lastVal = exprs.items[exprs.count - 1];
    if (lastVal != NULL && lastVal->type == NODE_IF &&
        lastVal->as.ifStmt.elseBranch == NULL) {
      errorAt(
          &parser.previous, ERR_SYNTAX,
          "Statement modifiers aren't allowed on 'let' declarations.",
          "Try using a standard if-block, or a ternary (if...else) instead.");
    }
    if (exprs.count != 1 && exprs.count != names.count) {
      errorAt(
          &parser.previous, ERR_SYNTAX, "Mismatch in assignment counts.",
          "When declaring multiple variables, you must provide exactly 1 value "
          "(to copy to all), or exactly match the number of variables.");
    }

    Node *node = newLetNode(names.items, names.count, exprs.items, exprs.count,
                            names.items[0].line);
    freeNodeArray(&exprs);
    freeTokenArray(&names);
    return node;
  } else if (match(TOKEN_EQUAL)) {
    // --- FOREIGN SYNTAX INTERCEPT: '=' instead of 'be' ---
    errorAt(&parser.previous, ERR_SYNTAX,
            "It looks like you used '=' to assign a variable.",
            "In MOON, we use the word 'be' for new variables. Try changing '=' "
            "to 'be' (e.g., 'let x be 10').");

    // Graceful Recovery: We pretend they typed 'be' and parse the expressions
    // anyway! This prevents a cascade of confusing follow-up errors.
    NodeArray exprs;
    initNodeArray(&exprs);
    do {
      writeNodeArray(&exprs, expression());
    } while (match(TOKEN_COMMA));

    freeNodeArray(&exprs);
    freeTokenArray(&names);
    return NULL; // Return NULL so we don't generate bad bytecode
  }

  Token rootName = names.items[0];

  // Only trigger function parsing if the next token is a valid word, keyword,
  // or punctuation!
  if (check(TOKEN_LEFT_PAREN) || check(TOKEN_COLON) ||
      check(TOKEN_IDENTIFIER) ||
      (parser.current.type >= TOKEN_ADD && parser.current.type <= TOKEN_WITH)) {
    if (names.count > 1) {
      errorAt(&parser.previous, ERR_SYNTAX,
              "I can't declare multiple functions at once.",
              "Function declarations must happen one at a time.");
      freeTokenArray(&names);
      return NULL;
    }

    int line = rootName.line;
    TokenArray parameters;
    initTokenArray(&parameters);
    TokenArray paramTypes;       // NEW
    initTokenArray(&paramTypes); // NEW

    char mangled[1024] = {0};
    strncat(mangled, rootName.start, rootName.length);

    if (check(TOKEN_COLON)) {
      strcat(mangled, "$0");
    } else {
      while (!check(TOKEN_COLON) && !check(TOKEN_EOF) &&
             !check(TOKEN_NEWLINE)) {
        if (match(TOKEN_LEFT_PAREN)) {
          int segmentArity = 0;
          if (!check(TOKEN_RIGHT_PAREN)) {
            do {
              consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
                          "I was expecting a parameter name here.",
                          "Provide a name for the function parameter.");
              writeTokenArray(&parameters, parser.previous);

              // --- NEW: THE TYPE EXTRACTION ---
              if (match(TOKEN_COLON)) {
                consumeHint(TOKEN_IDENTIFIER, ERR_TYPE,
                            "I was expecting a type name here.",
                            "Type annotations must specify a valid blueprint "
                            "type (e.g., 'name: String').");

                writeTokenArray(&paramTypes, parser.previous);
              } else {
                // If no type is provided, insert a dummy 'Any' token
                Token anyToken = {.type = TOKEN_IDENTIFIER,
                                  .start = "Any",
                                  .length = 3,
                                  .line = parser.previous.line};
                writeTokenArray(&paramTypes, anyToken);
              }
              // --------------------------------

              segmentArity++;
            } while (match(TOKEN_COMMA));
          }
          consumeHint(TOKEN_RIGHT_PAREN, ERR_SYNTAX,
                      "I couldn't find the closing parenthesis ')'.",
                      "Make sure to close your parameter list.");
          char buf[16];
          sprintf(buf, "$%d", segmentArity);
          strcat(mangled, buf);
        } else {
          advance();
          strcat(mangled, "_");
          strncat(mangled, parser.previous.start, parser.previous.length);
        }
      }
    }

    consumeHint(
        TOKEN_COLON, ERR_SYNTAX, "I was expecting a colon ':' here.",
        "Function signatures must end with a colon before the body begins.");

    Token funcStart = parser.previous; // <- capture the colon
    TokenType terminators[] = {TOKEN_END};
    Node *body = block(terminators, 1);
    consumeBlockEnd(funcStart, "function");

    Token finalName = rootName;
    finalName.start = my_strdup(mangled);
    finalName.length = strlen(mangled);

    // Pass paramTypes.items into the constructor!
    Node *node = newFunctionNode(finalName, parameters.items, paramTypes.items,
                                 parameters.count, body, line);
    freeTokenArray(&parameters);
    freeTokenArray(&paramTypes); // Free the temp buffer!
    freeTokenArray(&names);
    return node;
  }

  errorAt(&parser.previous, ERR_SYNTAX, "This declaration is confusing me.",
          "Use 'let x be 10' for variables, or provide a valid function "
          "signature (e.g., 'let jump():').");

  freeTokenArray(&names);
  return NULL;
}

static Node *grouping() {
  groupingDepth++; // Going deeper...

  // --- THE STICKY SCOPE FIX ---
  // Save the current subject on the C call stack before evaluating the inside
  Node *previousSubject = currentStickySubject;

  Node *expr = expression();

  // Restore the outside subject now that we are done!
  currentStickySubject = previousSubject;
  // ----------------------------

  groupingDepth--; // Coming back out!

  consumeHint(TOKEN_RIGHT_PAREN, ERR_SYNTAX,
              "I couldn't find the closing parenthesis ')'.",
              "Make sure you close any opened parentheses in your math, logic, "
              "or function calls.");
  return expr;
}

static Node *call(Node *callee) {
  int line = parser.previous.line;
  NodeArray args;
  initNodeArray(&args);

  groupingDepth++; // Protect the arguments!
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      writeNodeArray(&args, expression());
    } while (match(TOKEN_COMMA));
  }
  groupingDepth--; // Coming back out!

  consumeHint(TOKEN_RIGHT_PAREN, ERR_SYNTAX,
              "I couldn't find the closing parenthesis ')'.",
              "Make sure you close any opened parentheses in your math, logic, "
              "or function calls.");

  Node *node = newCallNode(callee, args.items, args.count, line);
  freeNodeArray(&args);
  return node;
}

static Node *declaration() {
  ignoreNewlines();
  if (check(TOKEN_EOF))
    return NULL;

  Node *decl;
  if (match(TOKEN_TYPE)) {    // <--- NEW
    decl = typeDeclaration(); // <--- NEW
  } else if (match(TOKEN_LET)) {
    decl = letDeclaration();
  } else {
    decl = statement();
  }

  if (parser.panicMode)
    synchronize();
  return decl;
}

// ==========================================
// 7. RULES TABLE & ENTRY
// ==========================================

ParseRule rules[] = {
    [TOKEN_AS] = {NULL, castExpression, PREC_CAST},
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_LEFT_BRACE] = {dict, instantiate, PREC_CALL}, // <--- Updated!
    [TOKEN_WITH] = {NULL, instantiateWith, PREC_CALL},   // <--- New!
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {explicitSticky, binary, PREC_FACTOR},
    [TOKEN_MOD] = {NULL, binary, PREC_FACTOR},
    [TOKEN_IS] = {stickyPrefix, binary, PREC_EQUALITY},
    [TOKEN_EQUAL_EQUAL] = {stickyPrefix, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {stickyPrefix, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {stickyPrefix, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {stickyPrefix, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {stickyPrefix, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {stickyPrefix, binary, PREC_COMPARISON},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_STRING_OPEN] = {interpolation, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_UNLESS] = {NULL, NULL, PREC_NONE},
    [TOKEN_NOT] = {unary, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_TO] = {NULL, range, PREC_RANGE},
    [TOKEN_LEFT_BRACKET] = {list, subscript, PREC_CALL},
    [TOKEN_DOT] = {NULL, dot, PREC_CALL},
    [TOKEN_POSSESSIVE] = {NULL, possessive, PREC_CALL},
    [TOKEN_END] = {endKeyword, NULL, PREC_NONE},

    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *getRule(TokenType type) { return &rules[type]; }

Node *parseSource(const char *source) {
  hoistPhrases(source);

  parser.hadError = false;
  parser.panicMode = false;
  advance();

  NodeArray statements;
  initNodeArray(&statements);

  while (!match(TOKEN_EOF)) {
    writeNodeArray(&statements, declaration());
  }

  Node *node = (parser.hadError)
                   ? NULL
                   : newBlockNode(statements.items, statements.count, 0);
  freeNodeArray(&statements);
  return node;
}
