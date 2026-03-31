#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "object.h"
#include "parser.h"
#include "scanner.h"
#include "sigtrie.h"

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
    error("Cannot use a complex expression as a sticky subject.");
    return NULL;
  }
}

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

void hoistPhrases(const char *source) {
  initSignatureTable();
  initScanner(source);

  Token token = scanToken();
  while (token.type != TOKEN_EOF) {
    if (token.type == TOKEN_LET) {
      Token nameToken = scanToken();
      if (nameToken.type == TOKEN_IDENTIFIER) {
        Token next = scanToken();

        if (next.type != TOKEN_BE && next.type != TOKEN_COMMA) {
          char mangled[1024] = {0};
          strncat(mangled, nameToken.start, nameToken.length);

          // THE 0-ARITY FIX: Did it immediately hit a colon?
          if (next.type == TOKEN_COLON) {
            strcat(mangled, "$0");
          } else {
            while (next.type != TOKEN_COLON && next.type != TOKEN_EOF &&
                   next.type != TOKEN_NEWLINE) {
              if (next.type == TOKEN_LEFT_PAREN) {
                int arity = 0;
                Token peek = scanToken();
                if (peek.type != TOKEN_RIGHT_PAREN) {
                  arity = 1; // If it's not empty, there's at least 1 argument!
                  while (peek.type != TOKEN_RIGHT_PAREN &&
                         peek.type != TOKEN_EOF) {
                    if (peek.type == TOKEN_COMMA) {
                      arity++; // Only increment arity when we see a comma!
                    }
                    peek = scanToken();
                  }
                }
                char buf[16];
                sprintf(buf, "$%d", arity);
                strcat(mangled, buf);
              } else {
                strcat(mangled, "_");
                strncat(mangled, next.start, next.length);
              }
              next = scanToken();
            }
          }

          char root[256] = {0};
          snprintf(root, nameToken.length + 1, "%.*s", nameToken.length,
                   nameToken.start);
          insertSignature(root, mangled);
        }
      }
    }

    token = scanToken();
  }

  // --- ADD THIS LINE ---
  // The Rewind: Reset the scanner back to the top of the file for the real
  // pass!
  initScanner(source);
}

// ==========================================
// 3. ERROR HANDLING
// ==========================================

void errorAt(Token *token, const char *message) {
  if (parser.panicMode)
    return;
  parser.panicMode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type != TOKEN_ERROR) {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

void error(const char *message) { errorAt(&parser.previous, message); }
void errorAtCurrent(const char *message) { errorAt(&parser.current, message); }

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
  advance();

  PrefixFn prefixRule = getRule(parser.previous.type)->prefix;

  if (prefixRule == NULL) {
    error("Expect expression.");
    return NULL;
  }

  Node *leftNode = prefixRule();

  while (precedence <= getRule(parser.current.type)->precedence) {
    // --- THE BLINDFOLD CHECK ---
    // This is the line you missed! It stops the Pratt parser from eating
    // labels.
    if (isExpectedLabel())
      break;

    advance();
    InfixFn infixRule = getRule(parser.previous.type)->infix;
    leftNode = infixRule(leftNode);
  }

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
    Token notToken = {
        .type = TOKEN_NOT, .start = "not", .length = 3, .line = line};
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
      errorAtCurrent("Expect end of string interpolation (` or \").");
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
      error("Incomplete phrasal function. Missing expected labels.");
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
    error("Invalid phrasing. Did not match any known function signature.");
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

static Node *stickyPrefix() {
  if (currentStickySubject == NULL) {
    error("Missing left operand, and no sticky subject is active.");
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
    Token notToken = {TOKEN_NOT, "not", 3, opToken.line};
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
    Token notToken = {TOKEN_NOT, "not", 3, opToken.line};
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
  consume(TOKEN_RIGHT_BRACKET, "Expect a ']' after list.");

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
        error("Expect property name (identifier or string) in dictionary.");
        break;
      }

      // 2. The Colon
      consume(TOKEN_COLON, "Expect ':' after property name.");

      // 3. The Value
      Node *valueNode = expression();

      writeNodeArray(&keys, keyNode);
      writeNodeArray(&values, valueNode);
    } while (match(TOKEN_COMMA));
  }

  groupingDepth--; // Coming back out!

  ignoreNewlines();
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after dictionary properties.");

  Node *node = newDictNode(keys.items, values.items, keys.count, line);
  freeNodeArray(&keys);
  freeNodeArray(&values);
  return node;
}

static Node *instantiate(Node *left) {
  int line = parser.previous.line;
  TokenArray propNames;
  initTokenArray(&propNames);
  NodeArray values;
  initNodeArray(&values);

  groupingDepth++; // Protect from blindfolds
  if (!check(TOKEN_RIGHT_BRACE)) {
    do {
      ignoreNewlines();
      if (check(TOKEN_RIGHT_BRACE))
        break;

      consume(TOKEN_IDENTIFIER, "Expect property name.");
      writeTokenArray(&propNames, parser.previous);
      consume(TOKEN_COLON, "Expect ':' after property name.");
      writeNodeArray(&values, expression());
    } while (match(TOKEN_COMMA));
  }
  groupingDepth--;

  ignoreNewlines();
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after instance properties.");

  Node *node = newInstantiateNode(left, propNames.items, values.items,
                                  propNames.count, line);
  freeTokenArray(&propNames);
  freeNodeArray(&values);
  return node;
}

static Node *instantiateWith(Node *left) {
  int line = parser.previous.line;
  TokenArray propNames;
  initTokenArray(&propNames);
  NodeArray values;
  initNodeArray(&values);

  if (!check(TOKEN_END)) {
    do {
      ignoreNewlines();
      if (check(TOKEN_END))
        break;

      consume(TOKEN_IDENTIFIER, "Expect property name.");
      writeTokenArray(&propNames, parser.previous);
      consume(TOKEN_COLON, "Expect ':' after property name.");
      writeNodeArray(&values, expression());
    } while (match(TOKEN_COMMA));
  }

  ignoreNewlines();
  consume(TOKEN_END, "Expect 'end' after 'with' block.");

  Node *node = newInstantiateNode(left, propNames.items, values.items,
                                  propNames.count, line);
  freeTokenArray(&propNames);
  freeNodeArray(&values);
  return node;
}

static Node *subscript(Node *left) {
  int line = parser.previous.line;

  groupingDepth++; // Protect the index!
  Node *index = expression();
  groupingDepth--; // Coming back out!

  consume(TOKEN_RIGHT_BRACKET, "Expect ']' after index.");
  return newSubscriptNode(left, index, line);
}

static Node *dot(Node *left) {
  int line = parser.previous.line;

  // Moon's Dot Indexer! It expects an expression, not a literal name.
  // e.g. `list.0` or `list.i`
  Node *index = parsePrecedence(PREC_CALL);

  // It returns a SubscriptNode, completely identical to `list[i]`
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
  consume(TOKEN_IDENTIFIER, "Expect property name after 's.");
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
    Token notToken = {TOKEN_NOT, "not", 3, line};
    condition = newUnaryNode(notToken, condition, line);
  }

  Node *thenBranch = NULL, *elseBranch = NULL;

  // THE KEYWORD SWAP: 'if' uses 'else', 'unless' uses 'then'
  TokenType altToken = invert ? TOKEN_THEN : TOKEN_ELSE;

  if (match(TOKEN_COLON)) {
    TokenType thenEnds[] = {altToken, TOKEN_END};
    thenBranch = block(thenEnds, 2);

    if (match(altToken)) {
      if (match(TOKEN_IF)) {
        elseBranch = ifStatement(false);
      } else if (match(TOKEN_UNLESS)) {
        elseBranch = ifStatement(true);
      } else if (match(TOKEN_COLON)) {
        TokenType elseEnds[] = {TOKEN_END};
        elseBranch = block(elseEnds, 1);
        consume(TOKEN_END, "Expect 'end' after block.");
      } else {
        elseBranch = statement();
      }
    } else {
      consume(TOKEN_END, "Expect 'end' after block.");
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
    Token notToken = {TOKEN_NOT, "not", 3, line};
    condition = newUnaryNode(notToken, condition, line);
  }

  Node *body = NULL;
  loopingDepth++;

  if (match(TOKEN_COLON)) {
    TokenType terminators[] = {TOKEN_END};
    body = block(terminators, 1);
    consume(TOKEN_END, "Expect 'end' after loop.");
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
  consume(TOKEN_IDENTIFIER, "Expect variable name.");

  if (check(TOKEN_IN))
    advance();
  else if (check(TOKEN_FROM))
    advance();

  Node *sequence = expression();
  Node *body = NULL;

  loopingDepth++;
  if (match(TOKEN_COLON)) {
    TokenType terminators[] = {TOKEN_END};
    body = block(terminators, 1);
    consume(TOKEN_END, "Expect 'end' after loop.");
  } else {
    body = statement();
  }
  loopingDepth--;

  return newForNode(iteratorName, sequence, body, line);
}

static Node *parseLValue() {
  // Base Case: The root must be an identifier
  consume(TOKEN_IDENTIFIER, "Expect variable name for assignment.");
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
  consume(TOKEN_TO, "Expect 'to' after expressions to add.");

  // 3. The Target (L-Value)
  Node *target = parseLValue();

  // 4. THE DESUGARING (Build the Accumulator)
  Node *accumulator = cloneNode(target);
  if (accumulator == NULL) {
    error("Invalid target for 'add' statement.");
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
    Token notToken = {TOKEN_NOT, "not", 3, line};
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

  consume(TOKEN_TO, "Expect 'to' after target(s).");

  do {
    writeNodeArray(&values, expression());
  } while (match(TOKEN_COMMA));

  Node *lastVal = values.items[values.count - 1];
  if (lastVal->type == NODE_IF && lastVal->as.ifStmt.elseBranch == NULL) {
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

  // THE AST INVERSION
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
    error("Cannot use 'break' or 'quit' outside of a loop.");
  }
  return newBreakNode(parser.previous.line);
}

static Node *skipStatement() {
  if (loopingDepth == 0) {
    error("Cannot use 'skip' outside of a loop.");
  }
  return newSkipNode(parser.previous.line);
}

static Node *typeDeclaration() {
  int line = parser.previous.line;
  consume(TOKEN_IDENTIFIER, "Expect type name.");
  Token name = parser.previous;
  consume(TOKEN_COLON, "Expect ':' after type name.");

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

      consume(TOKEN_IDENTIFIER, "Expect property name.");
      writeTokenArray(&propertyNames, parser.previous);

      // If they type `be 300`, parse the 300. Otherwise, silently default to
      // `nil`.
      if (match(TOKEN_BE)) {
        writeNodeArray(&defaultValues, expression());
      } else {
        writeNodeArray(&defaultValues,
                       newLiteralNode(NIL_VAL, parser.previous.line));
      }
    } while (match(TOKEN_COMMA));
  }

  ignoreNewlines();
  consume(TOKEN_END, "Expect 'end' after type declaration.");

  Node *node = newTypeNode(name, propertyNames.items, defaultValues.items,
                           propertyNames.count, line);
  freeTokenArray(&propertyNames);
  freeNodeArray(&defaultValues);
  return node;
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
  } else if (match(TOKEN_SET)) {
    stmt = setStatement();
  } else if (match(TOKEN_ADD)) {
    stmt = addStatement();
  } else if (match(TOKEN_GIVE)) {
    stmt = giveStatement();
  } else {
    stmt = expressionStatement();
  }

  return stmt;
}

static Node *letDeclaration() {
  TokenArray names;
  initTokenArray(&names);

  do {
    consume(TOKEN_IDENTIFIER, "Expect variable name.");
    writeTokenArray(&names, parser.previous);
  } while (match(TOKEN_COMMA));

  if (match(TOKEN_BE)) {
    NodeArray exprs;
    initNodeArray(&exprs);

    do {
      writeNodeArray(&exprs, expression());
    } while (match(TOKEN_COMMA));

    Node *lastVal = exprs.items[exprs.count - 1];
    if (lastVal->type == NODE_IF && lastVal->as.ifStmt.elseBranch == NULL) {
      error("Cannot use statement modifiers on 'let' declarations.");
    }
    if (exprs.count != 1 && exprs.count != names.count) {
      error("Expression count must be 1, or exactly match variables.");
    }

    Node *node = newLetNode(names.items, names.count, exprs.items, exprs.count,
                            names.items[0].line);
    freeNodeArray(&exprs);
    freeTokenArray(&names);
    return node;
  }

  Token rootName = names.items[0];

  if (check(TOKEN_LEFT_PAREN) || check(TOKEN_COLON) ||
      (!check(TOKEN_BE) && !check(TOKEN_COMMA))) {
    if (names.count > 1) {
      error("Cannot declare multiple functions on the same line.");
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
      while (!check(TOKEN_COLON) && !check(TOKEN_EOF)) {
        if (match(TOKEN_LEFT_PAREN)) {
          int segmentArity = 0;
          if (!check(TOKEN_RIGHT_PAREN)) {
            do {
              consume(TOKEN_IDENTIFIER, "Expect parameter name.");
              writeTokenArray(&parameters, parser.previous);

              // --- NEW: THE TYPE EXTRACTION ---
              if (match(TOKEN_COLON)) {
                consume(TOKEN_IDENTIFIER, "Expect type name after ':'.");
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
          consume(TOKEN_RIGHT_PAREN, "Expect ')'.");
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

    consume(TOKEN_COLON, "Expect ':' before function body.");
    TokenType terminators[] = {TOKEN_END};
    Node *body = block(terminators, 1);
    consume(TOKEN_END, "Expect 'end' after function body.");

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

  error("Expect 'be' for variables or a valid function signature.");
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

  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
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

  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");

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
  insertSignature("show", "show$1");

  parser.hadError = false;
  parser.panicMode = false;
  advance();

  NodeArray statements;
  initNodeArray(&statements);

  while (!match(TOKEN_EOF)) {
    writeNodeArray(&statements, declaration());
  }

  freeSignatureTable();
  Node *node = (parser.hadError)
                   ? NULL
                   : newBlockNode(statements.items, statements.count, 0);
  freeNodeArray(&statements);
  return node;
}
