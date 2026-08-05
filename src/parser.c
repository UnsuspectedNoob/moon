#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "debug.h"
#include "error.h"
#include "memory.h"
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
  const char *labelText;
  int labelLength;
} ExpectedLabel;

static ExpectedLabel expectedLabelStack[256];
static int expectedLabelCount = 0;
static int groupingDepth = 0; // Tracks if we are inside () or []

static int loopingDepth = 0;
static bool isReservedKeyword(TokenType type);
static Token makeHiddenToken(const char *text, int line);

// --- ADD THESE TWO LINES ---
static int parseDepth = 0;
#define MAX_AST_DEPTH 1024
// ---------------------------

static bool isExpectedLabel() {
  if (expectedLabelCount == 0)
    return false;

  // Check if it matches an expected label AT THE CURRENT DEPTH
  for (int i = 0; i < expectedLabelCount; i++) {
    if (expectedLabelStack[i].hash == parser.currentHash &&
        expectedLabelStack[i].depth == groupingDepth &&
        parser.current.length == expectedLabelStack[i].labelLength &&
        memcmp(parser.current.start, expectedLabelStack[i].labelText,
               parser.current.length) == 0) {
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

// --------------------------------------

// You will need to expose your readFile helper from main.c or rewrite a quick
// one here

static bool canStartExpression(TokenType type) {
  switch (type) {
  case TOKEN_NEWLINE:
  case TOKEN_EOF:
  case TOKEN_RIGHT_PAREN:
  case TOKEN_RIGHT_BRACKET:
  case TOKEN_RIGHT_BRACE:
  case TOKEN_COMMA:
  case TOKEN_COLON:
  case TOKEN_DOT:
  case TOKEN_POSSESSIVE:
  case TOKEN_PLUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
  case TOKEN_MOD:
  case TOKEN_ADD_INPLACE:
  case TOKEN_EQUAL:
  case TOKEN_EQUAL_EQUAL:
  case TOKEN_BANG_EQUAL:
  case TOKEN_GREATER:
  case TOKEN_GREATER_EQUAL:
  case TOKEN_LESS:
  case TOKEN_LESS_EQUAL:
  case TOKEN_IS:
  case TOKEN_AND:
  case TOKEN_OR:
  case TOKEN_AS:
  case TOKEN_WITH:
  case TOKEN_IN:
  case TOKEN_FROM:
  case TOKEN_EACH:
  case TOKEN_KEEP:
  case TOKEN_END:
  case TOKEN_ELSE:
  case TOKEN_BE:
  case TOKEN_TO:
  case TOKEN_IF:
  case TOKEN_UNLESS:
  case TOKEN_THEN:
  case TOKEN_BY:
  case TOKEN_SET:
  case TOKEN_LET:
  case TOKEN_UPDATE:
  case TOKEN_GIVE:
  case TOKEN_BREAK:
  case TOKEN_QUIT:
  case TOKEN_SKIP:
  case TOKEN_WHILE:
  case TOKEN_UNTIL:
  case TOKEN_FOR:
  case TOKEN_TYPE:
  case TOKEN_LOAD:
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

static void consumeStatementEnd() {
  if (check(TOKEN_NEWLINE) || check(TOKEN_EOF) || check(TOKEN_END) ||
      check(TOKEN_ELSE) || check(TOKEN_THEN) ||
      parser.previous.type == TOKEN_NEWLINE) {
    return;
  }

  errorAt(&parser.current, ERR_SYNTAX,
          "I was expecting a newline after this statement.",
          "Make sure you only write one statement per line. If you are trying "
          "to write multiple statements, they must be separated by a newline.");
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
    if (printScanFlag) {
      if (parser.current.line != parser.previous.line &&
          parser.previous.line != 0) {
        printf("%4d ", parser.current.line);
      } else if (parser.previous.line == 0) {
        printf("%4d ", parser.current.line);
      } else {
        printf("   | ");
      }
      printf("%-22s '", getTokenTypeName(parser.current.type));
      if (parser.current.type == TOKEN_NEWLINE) {
        printf("\\n");
      } else if (parser.current.type == TOKEN_EOF) {
        // empty quotes ''
      } else {
        printEscapedLexeme(parser.current.start, parser.current.length);
      }
      printf("'\n");
    }
    if (parser.current.type != TOKEN_ERROR) {
      parser.currentHash =
          hashString(parser.current.start, parser.current.length);
      break;
    }

    // Use the stowed errorMessage, otherwise fallback to the token text
    const char *message = parser.current.errorMessage != NULL
                              ? parser.current.errorMessage
                              : parser.current.start;

    errorAt(&parser.current, ERR_SYNTAX, message, NULL);
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
  PREC_RANGE,
  PREC_OR,
  PREC_AND,
  PREC_COMPARISON,
  PREC_TERM,
  PREC_FACTOR,
  PREC_CAST,
  PREC_UNARY,
  PREC_PHRASE,
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
static Precedence getInfixPrecedence(TokenType type);
static Node *parsePropertySignatureBody(Token receiverName, Node *receiverType,
                                        int line);

static Precedence getInfixPrecedence(TokenType type) {
  if (type == TOKEN_IDENTIFIER) {
    if (hasInfixSignature(parser.current.start, parser.current.length)) {
      return PREC_PHRASE;
    }
    return PREC_NONE;
  }
  return getRule(type)->precedence;
}

static void validatePureExpression(Node *node, const char *context) {
  if (node == NULL)
    return;
  if (node->type == NODE_IF && node->as.ifStmt.elseBranch == NULL) {
    char message[256];
    snprintf(message, sizeof(message),
             "Statement modifiers are not allowed %s.", context);
    errorAt(&parser.previous, ERR_SYNTAX, message,
             "If you meant to use a ternary, add an 'else' branch.");
  } else if (node->type == NODE_PHRASAL_CALL) {
    if (node->as.phrasalCall.argCount > 0) {
      validatePureExpression(
          node->as.phrasalCall.arguments[node->as.phrasalCall.argCount - 1],
          context);
    }
  } else if (node->type == NODE_GROUPING) {
    validatePureExpression(node->as.singleExpr.expression, context);
  }
}

static Node *parsePrecedence(Precedence precedence) {
  // 1. Tracks recursive depth (Right-heavy trees)
  parseDepth++;
  if (parseDepth > MAX_AST_DEPTH) {
    errorAt(&parser.previous, ERR_SYNTAX,
            "This expression is too deeply nested and I'm losing track of it!",
            "Try breaking this complex calculation into smaller steps using "
            "intermediate variables.");
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

  while (precedence <= getInfixPrecedence(parser.current.type)) {
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
  Node *expr = parsePrecedence(PREC_RANGE);

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
    Token notToken = {TOKEN_NOT, "not", 3, line, 0, NULL};
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
  int prefixLen = 1;
  int totalQuotes = 2;
  if (parser.previous.start[0] == '\'') {
    prefixLen = 3;
    totalQuotes = 6;
  }
  ObjString *str = copyStringUnescaped(parser.previous.start + prefixLen,
                                       parser.previous.length - totalQuotes);
  return newLiteralNode(OBJ_VAL(str), parser.previous.line);
}

static Node *extractInterpolationString(Token token) {
  int prefixLen = 1;
  int suffixLen = 1;

  if (token.type == TOKEN_STRING_OPEN) {
    if (token.start[0] == '\'')
      prefixLen = 3;
    else
      prefixLen = 1;
    suffixLen = 1; // ends with `
  } else if (token.type == TOKEN_STRING_CLOSE) {
    prefixLen = 1; // starts with `
    if (token.start[token.length - 1] == '\'')
      suffixLen = 3;
    else
      suffixLen = 1;
  } else if (token.type == TOKEN_STRING_MIDDLE) {
    prefixLen = 1;
    suffixLen = 1;
  }

  ObjString *str = copyStringUnescaped(token.start + prefixLen,
                                       token.length - prefixLen - suffixLen);
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
    Node *expr = expression();
    validatePureExpression(expr, "inside a string interpolation");
    writeNodeArray(&parts, expr);

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
  const char *start = parser.previous.start;
  double value;

  int i = 0;
  while (start[i] == '0')
    i++;

  if (start[i] == 'x' || start[i] == 'X') {
    value = (double)strtoull(start + i + 1, NULL, 16);
  } else if (start[i] == 'b' || start[i] == 'B') {
    value = (double)strtoull(start + i + 1, NULL, 2);
  } else {
    value = strtod(start, NULL);
  }

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

static Node *implicitIt() {
  Token itToken = makeHiddenToken(" it", parser.previous.line);
  return newVariableNode(itToken, parser.previous.line);
}

static bool matchSignatureLookahead(TrieNode *currentNode) {
  Scanner savedScanner = scanner;
  Parser savedParser = parser;

  while (currentNode->childCount > 0) {
    uint32_t nextHash = parser.currentHash;
    TrieNode *matchedLabel = NULL;
    bool expectsArgument = false;

    for (int i = 0; i < currentNode->childCount; i++) {
      TrieNode *child = currentNode->children[i];
      if (child->type == NODE_LABEL && child->labelHash == nextHash &&
          parser.current.length == child->labelLength &&
          memcmp(parser.current.start, child->labelName, child->labelLength) ==
              0) {
        matchedLabel = child;
      } else if (child->type == NODE_ARGUMENT) {
        expectsArgument = true;
      }
    }

    if (matchedLabel != NULL) {
      advance();
      currentNode = matchedLabel;
      continue;
    }

    if (expectsArgument && canStartExpression(parser.current.type)) {
      uint32_t possibleLabels[256];
      const char *possibleLabelNames[256];
      int possibleLabelLengths[256];
      int possibleLabelCount = 0;

      for (int i = 0; i < currentNode->childCount; i++) {
        if (currentNode->children[i]->type == NODE_ARGUMENT) {
          TrieNode *ac = currentNode->children[i];
          for (int j = 0; j < ac->childCount; j++) {
            if (ac->children[j]->type == NODE_LABEL) {
              possibleLabels[possibleLabelCount] = ac->children[j]->labelHash;
              possibleLabelNames[possibleLabelCount] =
                  ac->children[j]->labelName;
              possibleLabelLengths[possibleLabelCount] =
                  ac->children[j]->labelLength;
              possibleLabelCount++;
            }
          }
        }
      }

      int argGroupingDepth = 0;

      while (parser.current.type != TOKEN_EOF &&
             parser.current.type != TOKEN_NEWLINE &&
             parser.current.type != TOKEN_END) {
        if (argGroupingDepth == 0) {
          bool hitLabel = false;
          for (int i = 0; i < possibleLabelCount; i++) {
            if (parser.currentHash == possibleLabels[i] &&
                parser.current.length == possibleLabelLengths[i] &&
                memcmp(parser.current.start, possibleLabelNames[i],
                       possibleLabelLengths[i]) == 0) {
              hitLabel = true;
              break;
            }
          }
          if (hitLabel)
            break;
        }

        if (parser.current.type == TOKEN_LEFT_PAREN ||
            parser.current.type == TOKEN_LEFT_BRACKET ||
            parser.current.type == TOKEN_LEFT_BRACE) {
          argGroupingDepth++;
        } else if (parser.current.type == TOKEN_RIGHT_PAREN ||
                   parser.current.type == TOKEN_RIGHT_BRACKET ||
                   parser.current.type == TOKEN_RIGHT_BRACE) {
          argGroupingDepth--;
          if (argGroupingDepth < 0) {
            break;
          }
        }
        advance();
      }

      TrieNode *matchedArgChild = NULL;
      for (int i = 0; i < currentNode->childCount; i++) {
        if (currentNode->children[i]->type == NODE_ARGUMENT) {
          TrieNode *ac = currentNode->children[i];
          bool hasLabel = false;
          for (int j = 0; j < ac->childCount; j++) {
            if (ac->children[j]->type == NODE_LABEL &&
                ac->children[j]->labelHash == parser.currentHash &&
                parser.current.length == ac->children[j]->labelLength &&
                memcmp(parser.current.start, ac->children[j]->labelName,
                       parser.current.length) == 0) {
              hasLabel = true;
              break;
            }
          }
          if (hasLabel) {
            matchedArgChild = ac;
            break;
          } else if (ac->terminalType != TERMINAL_NONE) {
            matchedArgChild = ac;
          }
        }
      }

      if (matchedArgChild != NULL) {
        currentNode = matchedArgChild;
        continue;
      }
    }
    break;
  }

  bool success = currentNode->terminalType != TERMINAL_NONE;

  scanner = savedScanner;
  parser = savedParser;

  return success;
}

static Node *parsePhrasalCall(TrieNode *startNode, Token rootToken,
                              bool isMethod, Node *methodTarget) {
  NodeArray args;
  initNodeArray(&args);

  Token phraseTokens[16];
  int phraseTokenCount = 0;
  phraseTokens[phraseTokenCount++] = rootToken;

  TrieNode *currentNode = startNode;

  while (currentNode->childCount > 0) {
    uint32_t nextHash = parser.currentHash;
    TrieNode *matchedLabel = NULL;
    bool expectsArgument = false;

    for (int i = 0; i < currentNode->childCount; i++) {
      TrieNode *child = currentNode->children[i];
      if (child->type == NODE_LABEL && child->labelHash == nextHash &&
          parser.current.length == child->labelLength &&
          memcmp(parser.current.start, child->labelName, child->labelLength) ==
              0) {
        matchedLabel = child;
      } else if (child->type == NODE_ARGUMENT) {
        expectsArgument = true;
      }
    }

    if (matchedLabel != NULL) {
      if (phraseTokenCount < 16) {
        phraseTokens[phraseTokenCount++] = parser.current;
      }
      advance();
      currentNode = matchedLabel;
      continue;
    }

    if (expectsArgument) {
      if (!canStartExpression(parser.current.type) &&
          currentNode->terminalType != TERMINAL_NONE) {
        break;
      }

      int labelsPushed = 0;
      for (int i = 0; i < currentNode->childCount; i++) {
        if (currentNode->children[i]->type == NODE_ARGUMENT) {
          TrieNode *ac = currentNode->children[i];
          for (int j = 0; j < ac->childCount; j++) {
            if (ac->children[j]->type == NODE_LABEL) {
              if (expectedLabelCount >= 256) {
                errorAt(&parser.previous, ERR_SYNTAX,
                        "This expression is too deeply nested and I'm losing "
                        "track of it!",
                        "");
                for (int k = 0; k < args.count; k++)
                  freeNode(args.items[k]);
                freeNodeArray(&args);
                return NULL;
              }
              expectedLabelStack[expectedLabelCount].hash =
                  ac->children[j]->labelHash;
              expectedLabelStack[expectedLabelCount].depth = groupingDepth;
              expectedLabelStack[expectedLabelCount].labelText =
                  ac->children[j]->labelName;
              expectedLabelStack[expectedLabelCount].labelLength =
                  ac->children[j]->labelLength;
              expectedLabelCount++;
              labelsPushed++;
            }
          }
        }
      }

      NodeArray tempArgs;
      initNodeArray(&tempArgs);
      Node *arg = NULL;
      bool hasMultiArgChild = false;
      for (int i = 0; i < currentNode->childCount; i++) {
        if (currentNode->children[i]->type == NODE_ARGUMENT &&
            currentNode->children[i]->arity > 1) {
          hasMultiArgChild = true;
          break;
        }
      }

      if (hasMultiArgChild && labelsPushed == 0 && check(TOKEN_LEFT_PAREN)) {
        arg = parsePrecedence(PREC_CALL);
      } else {
        arg = expression();
      }

      if (arg == NULL) {
        for (int i = 0; i < tempArgs.count; i++)
          freeNode(tempArgs.items[i]);
        freeNodeArray(&tempArgs);
        break;
      }

      if (arg != NULL) {
        if (arg->type == NODE_TUPLE) {
          for (int j = 0; j < arg->as.tuple.count; j++) {
            writeNodeArray(&tempArgs, arg->as.tuple.items[j]);
          }
          FREE_ARRAY(Node *, arg->as.tuple.items, arg->as.tuple.count);
          FREE(Node, arg);
        } else if (arg->type == NODE_GROUPING) {
          writeNodeArray(&tempArgs, arg->as.singleExpr.expression);
          FREE(Node, arg);
        } else {
          writeNodeArray(&tempArgs, arg);
        }
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
        for (int i = 0; i < tempArgs.count; i++)
          writeNodeArray(&args, tempArgs.items[i]);
        freeNodeArray(&tempArgs);
        currentNode = matchedArgChild;
        continue;
      } else {
        for (int i = 0; i < tempArgs.count; i++)
          freeNode(tempArgs.items[i]);
        freeNodeArray(&tempArgs);
        break;
      }
    }
    break;
  }

  if (currentNode->terminalType == TERMINAL_NONE) {
    errorAt(&parser.previous, ERR_SYNTAX,
            "This phrasal function looks incomplete.",
            "You started a phrase but didn't finish it. Check the function's "
            "signature to see what words or arguments are missing.");
    for (int i = 0; i < args.count; i++)
      freeNode(args.items[i]);
    freeNodeArray(&args);
    return NULL;
  }

  Token mangledToken = rootToken;
  mangledToken.start = my_strdup(currentNode->mangledName);
  mangledToken.length = strlen(currentNode->mangledName);

  for (int i = 0; i < args.count - 1; i++) {
    validatePureExpression(args.items[i],
                           "as a non-final argument in a phrase");
  }

  Node *node;
  if (currentNode->terminalType == TERMINAL_VARIABLE) {
    if (args.count > 0) {
      errorAt(&parser.previous, ERR_SYNTAX, "Variables cannot take arguments.",
              "");
      for (int i = 0; i < args.count; i++)
        freeNode(args.items[i]);
      freeNodeArray(&args);
      return NULL;
    }
    if (isMethod) {
      node = newPropertyNode(methodTarget, mangledToken, rootToken.line);
    } else {
      node = newVariableNode(mangledToken, rootToken.line);
    }
  } else {
    if (isMethod) {
      node = newPhrasalMethodCallNode(methodTarget, mangledToken, args.items,
                                      args.count, rootToken.line);
    } else {
      node = newPhrasalCallNode(mangledToken, args.items, args.count,
                                phraseTokens, phraseTokenCount, rootToken.line);
    }
  }
  freeNodeArray(&args);
  return node;
}

static Node *variable() {
  Token rootToken = parser.previous;

  char rootWord[256] = {0};
  snprintf(rootWord, sizeof(rootWord), "%.*s", rootToken.length,
           rootToken.start);

  TrieNode *currentNode = getSignatureTrie(rootWord);

  if (currentNode == NULL) {
    if (rootToken.length == 2 && memcmp(rootToken.start, "my", 2) == 0) {
      if (parser.current.type == TOKEN_IDENTIFIER) {
        Token firstPropToken = parser.current;
        char propRoot[256] = {0};
        snprintf(propRoot, sizeof(propRoot), "%.*s", firstPropToken.length,
                 firstPropToken.start);
        TrieNode *propNode = getSignatureTrie(propRoot);

        if (propNode != NULL) {
          Scanner savedScanner = scanner;
          Parser savedParser = parser;
          advance(); // consume the first word

          if (matchSignatureLookahead(propNode)) {
            Node *myVar = newVariableNode(rootToken, rootToken.line);
            Node *phrasal =
                parsePhrasalCall(propNode, firstPropToken, true, myVar);
            if (phrasal != NULL)
              return phrasal;
          }

          scanner = savedScanner;
          parser = savedParser;
        }

        // Fallback to single token
        advance();
        Token propertyToken = parser.previous;
        Node *myVar = newVariableNode(rootToken, rootToken.line);
        return newPropertyNode(myVar, propertyToken, propertyToken.line);
      }
    }
    return newVariableNode(rootToken, rootToken.line);
  }

  Scanner savedScanner = scanner;
  Parser savedParser = parser;

  bool hasLookahead = matchSignatureLookahead(currentNode);

  if (hasLookahead) {
    Node *phrasal = parsePhrasalCall(currentNode, rootToken, false, NULL);
    if (phrasal != NULL) {
      return phrasal;
    }
  }

  scanner = savedScanner;
  parser = savedParser;

  if (rootToken.length == 2 && memcmp(rootToken.start, "my", 2) == 0) {
    if (check(TOKEN_IDENTIFIER)) {
      advance();
      Token propertyToken = parser.previous;
      Node *myVar = newVariableNode(rootToken, rootToken.line);
      return newPropertyNode(myVar, propertyToken, propertyToken.line);
    }
  }
  return newVariableNode(rootToken, rootToken.line);
}

static Node *explicitSticky() {
  Node *right = parsePrecedence(PREC_UNARY);
  return newSingleExprNode(NODE_BIND_STICKY, right, parser.previous.line);
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
    errorAt(&parser.previous, ERR_SYNTAX, "Invalid assignment target.",
            "You can only assign values to variables, properties, or list "
            "items.");
    return NULL;
  }
}

static Node *stickyPrefix() {
  Token opToken = parser.previous;

  // --- THE "IS NOT" FIX ---
  bool invert = false;
  if ((opToken.type == TOKEN_IS) && match(TOKEN_NOT)) {
    invert = true;
  }

  ParseRule *rule = getRule(opToken.type);
  Node *right = parsePrecedence((Precedence)(rule->precedence + 1));

  // Build the binary node using a sticky load node!
  Node *stickyNode = newBinaryNode(newLoadStickyNode(opToken.line), opToken,
                                   right, opToken.line);

  if (invert) {
    Token notToken = {TOKEN_NOT, "not", 3, opToken.line, 0, NULL};
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
  Node *right = parsePrecedence(PREC_CAST + 1);
  return newCastNode(left, right, line);
}

static Node *binary(Node *left) {
  Token opToken = parser.previous;

  // --- CHAINED COMPARISON LOGIC ---
  if (isComparison(opToken)) {
    if ((opToken.type == TOKEN_IS) && match(TOKEN_NOT)) {
      opToken.type = TOKEN_BANG_EQUAL; // Treat 'is not' natively as !=
    }

    ParseRule *rule = getRule(opToken.type);
    Node *right = parsePrecedence((Precedence)(rule->precedence + 1));

    if (left != NULL && left->type == NODE_CHAIN) {
      ParseRule *chainRule = getRule(left->as.chain.operators.items[0].type);
      if (chainRule->precedence == rule->precedence) {
        // Extend the existing chain!
        writeNodeArray(&left->as.chain.expressions, right);
        writeTokenArray(&left->as.chain.operators, opToken);

        if (right) {
          if (right->usesIt)
            left->usesIt = true;
          right->parent = left;
        }

        return left;
      }
    }

    // Start a new chain!
    Node *chain = newChainNode(left, opToken.line);
    writeNodeArray(&chain->as.chain.expressions, right);
    writeTokenArray(&chain->as.chain.operators, opToken);

    if (right) {
      if (right->usesIt)
        chain->usesIt = true;
      right->parent = chain;
    }

    return chain;
  }

  // --- STANDARD BINARY OPERATORS (+, -, *, etc) ---
  ParseRule *rule = getRule(opToken.type);
  Node *right = parsePrecedence((Precedence)(rule->precedence + 1));
  Node *binNode = newBinaryNode(left, opToken, right, opToken.line);

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

static Node *parsePhrasalInfixCall(TrieNode *argChild, Token rootToken,
                                   Node *left) {
  NodeArray args;
  initNodeArray(&args);

  if (left != NULL) {
    if (left->type == NODE_TUPLE) {
      for (int j = 0; j < left->as.tuple.count; j++) {
        writeNodeArray(&args, left->as.tuple.items[j]);
      }
      FREE_ARRAY(Node *, left->as.tuple.items, left->as.tuple.count);
      FREE(Node, left);
    } else if (left->type == NODE_GROUPING) {
      writeNodeArray(&args, left->as.singleExpr.expression);
      FREE(Node, left);
    } else {
      writeNodeArray(&args, left);
    }
  }

  Token phraseTokens[16];
  int phraseTokenCount = 0;
  phraseTokens[phraseTokenCount++] = rootToken;

  TrieNode *currentNode = argChild;

  while (currentNode->childCount > 0) {
    uint32_t nextHash = parser.currentHash;
    TrieNode *matchedLabel = NULL;
    bool expectsArgument = false;

    for (int i = 0; i < currentNode->childCount; i++) {
      TrieNode *child = currentNode->children[i];
      if (child->type == NODE_LABEL && child->labelHash == nextHash &&
          parser.current.length == child->labelLength &&
          memcmp(parser.current.start, child->labelName, child->labelLength) ==
              0) {
        matchedLabel = child;
      } else if (child->type == NODE_ARGUMENT) {
        expectsArgument = true;
      }
    }

    if (matchedLabel != NULL) {
      if (phraseTokenCount < 16) {
        phraseTokens[phraseTokenCount++] = parser.current;
      }
      advance();
      currentNode = matchedLabel;
      continue;
    }

    if (expectsArgument) {
      if (!canStartExpression(parser.current.type) &&
          currentNode->terminalType != TERMINAL_NONE) {
        break;
      }

      int labelsPushed = 0;
      for (int i = 0; i < currentNode->childCount; i++) {
        if (currentNode->children[i]->type == NODE_ARGUMENT) {
          TrieNode *ac = currentNode->children[i];
          for (int j = 0; j < ac->childCount; j++) {
            if (ac->children[j]->type == NODE_LABEL) {
              if (expectedLabelCount >= 256) {
                errorAt(&parser.previous, ERR_SYNTAX,
                        "This expression is too deeply nested and I'm losing "
                        "track of it!",
                        "");
                for (int k = 0; k < args.count; k++)
                  freeNode(args.items[k]);
                freeNodeArray(&args);
                return NULL;
              }
              expectedLabelStack[expectedLabelCount].hash =
                  ac->children[j]->labelHash;
              expectedLabelStack[expectedLabelCount].depth = groupingDepth;
              expectedLabelStack[expectedLabelCount].labelText =
                  ac->children[j]->labelName;
              expectedLabelStack[expectedLabelCount].labelLength =
                  ac->children[j]->labelLength;
              expectedLabelCount++;
              labelsPushed++;
            }
          }
        }
      }

      NodeArray tempArgs;
      initNodeArray(&tempArgs);
      Node *arg = NULL;
      bool hasMultiArgChild = false;
      for (int i = 0; i < currentNode->childCount; i++) {
        if (currentNode->children[i]->type == NODE_ARGUMENT &&
            currentNode->children[i]->arity > 1) {
          hasMultiArgChild = true;
          break;
        }
      }

      if (hasMultiArgChild && labelsPushed == 0 && check(TOKEN_LEFT_PAREN)) {
        arg = parsePrecedence(PREC_CALL);
      } else {
        arg = expression();
      }

      if (arg == NULL) {
        for (int i = 0; i < tempArgs.count; i++)
          freeNode(tempArgs.items[i]);
        freeNodeArray(&tempArgs);
        break;
      }

      if (arg->type == NODE_TUPLE) {
        for (int j = 0; j < arg->as.tuple.count; j++) {
          writeNodeArray(&tempArgs, arg->as.tuple.items[j]);
        }
        FREE_ARRAY(Node *, arg->as.tuple.items, arg->as.tuple.count);
        FREE(Node, arg);
      } else if (arg->type == NODE_GROUPING) {
        writeNodeArray(&tempArgs, arg->as.singleExpr.expression);
        FREE(Node, arg);
      } else {
        writeNodeArray(&tempArgs, arg);
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
        for (int i = 0; i < tempArgs.count; i++)
          writeNodeArray(&args, tempArgs.items[i]);
        freeNodeArray(&tempArgs);
        currentNode = matchedArgChild;
        continue;
      } else {
        for (int i = 0; i < tempArgs.count; i++)
          freeNode(tempArgs.items[i]);
        freeNodeArray(&tempArgs);
        break;
      }
    }
    break;
  }

  if (currentNode->terminalType == TERMINAL_NONE) {
    errorAt(&parser.previous, ERR_SYNTAX,
            "This phrasal function looks incomplete.",
            "You started a phrase but didn't finish it. Check the function's "
            "signature to see what words or arguments are missing.");
    for (int i = 0; i < args.count; i++)
      freeNode(args.items[i]);
    freeNodeArray(&args);
    return NULL;
  }

  Token mangledToken = rootToken;
  mangledToken.start = my_strdup(currentNode->mangledName);
  mangledToken.length = strlen(currentNode->mangledName);

  for (int i = 0; i < args.count - 1; i++) {
    validatePureExpression(args.items[i],
                           "as a non-final argument in a phrase");
  }

  Node *node = newPhrasalCallNode(mangledToken, args.items, args.count,
                                  phraseTokens, phraseTokenCount,
                                  rootToken.line);
  freeNodeArray(&args);
  return node;
}

static Node *phrasalInfix(Node *left) {
  Token rootToken = parser.previous;
  char rootWord[256] = {0};
  snprintf(rootWord, sizeof(rootWord), "%.*s", rootToken.length,
           rootToken.start);

  int leftArity = (left != NULL && left->type == NODE_TUPLE) ? left->as.tuple.count : 1;

  TrieNode *rootNode = getSignatureTrie(rootWord);
  if (rootNode != NULL) {
    for (int i = 0; i < rootNode->childCount; i++) {
      if (rootNode->children[i]->type == NODE_ARGUMENT &&
          rootNode->children[i]->isLeadingArg &&
          rootNode->children[i]->arity == leftArity) {
        TrieNode *argChild = rootNode->children[i];
        Scanner savedScanner = scanner;
        Parser savedParser = parser;

        if (matchSignatureLookahead(argChild)) {
          Node *phrasal = parsePhrasalInfixCall(argChild, rootToken, left);
          if (phrasal != NULL)
            return phrasal;
        }

        scanner = savedScanner;
        parser = savedParser;
      }
    }
  }

  errorAt(
      &parser.previous, ERR_SYNTAX,
      "Unexpected identifier following expression.",
      "If you meant to call a phrasal function, check that the signature is "
      "defined.");
  return left;
}

static Node *binaryInterceptor(Node *left) {
  Token opToken = parser.previous;
  char rootWord[256] = {0};
  snprintf(rootWord, sizeof(rootWord), "%.*s", opToken.length, opToken.start);

  int leftArity = (left != NULL && left->type == NODE_TUPLE) ? left->as.tuple.count : 1;

  TrieNode *rootNode = getSignatureTrie(rootWord);
  if (rootNode != NULL) {
    for (int i = 0; i < rootNode->childCount; i++) {
      if (rootNode->children[i]->type == NODE_ARGUMENT &&
          rootNode->children[i]->isLeadingArg &&
          rootNode->children[i]->arity == leftArity) {
        TrieNode *argChild = rootNode->children[i];
        Scanner savedScanner = scanner;
        Parser savedParser = parser;

        if (matchSignatureLookahead(argChild)) {
          Node *phrasal = parsePhrasalInfixCall(argChild, opToken, left);
          if (phrasal != NULL)
            return phrasal;
        }

        scanner = savedScanner;
        parser = savedParser;
      }
    }
  }

  return binary(left);
}

static Node *isInterceptor(Node *left) {
  Token opToken = parser.previous;
  int leftArity = (left != NULL && left->type == NODE_TUPLE) ? left->as.tuple.count : 1;

  TrieNode *rootNode = getSignatureTrie("is");
  if (rootNode != NULL) {
    for (int i = 0; i < rootNode->childCount; i++) {
      if (rootNode->children[i]->type == NODE_ARGUMENT &&
          rootNode->children[i]->isLeadingArg &&
          rootNode->children[i]->arity == leftArity) {
        TrieNode *argChild = rootNode->children[i];
        Scanner savedScanner = scanner;
        Parser savedParser = parser;

        if (matchSignatureLookahead(argChild)) {
          Node *phrasal = parsePhrasalInfixCall(argChild, opToken, left);
          if (phrasal != NULL)
            return phrasal;
        }

        scanner = savedScanner;
        parser = savedParser;
      }
    }
  }

  return binary(left);
}

static Node *andInterceptor(Node *left) {
  Token opToken = parser.previous;
  TrieNode *rootNode = getSignatureTrie("and");
  if (rootNode != NULL) {
    for (int i = 0; i < rootNode->childCount; i++) {
      if (rootNode->children[i]->type == NODE_ARGUMENT &&
          rootNode->children[i]->arity == 1) {
        TrieNode *argChild = rootNode->children[i];
        Scanner savedScanner = scanner;
        Parser savedParser = parser;

        if (matchSignatureLookahead(argChild)) {
          Node *phrasal = parsePhrasalInfixCall(argChild, opToken, left);
          if (phrasal != NULL)
            return phrasal;
        }

        scanner = savedScanner;
        parser = savedParser;
      }
    }
  }

  return and_(left);
}

static Node *orInterceptor(Node *left) {
  Token opToken = parser.previous;
  TrieNode *rootNode = getSignatureTrie("or");
  if (rootNode != NULL) {
    for (int i = 0; i < rootNode->childCount; i++) {
      if (rootNode->children[i]->type == NODE_ARGUMENT &&
          rootNode->children[i]->arity == 1) {
        TrieNode *argChild = rootNode->children[i];
        Scanner savedScanner = scanner;
        Parser savedParser = parser;

        if (matchSignatureLookahead(argChild)) {
          Node *phrasal = parsePhrasalInfixCall(argChild, opToken, left);
          if (phrasal != NULL)
            return phrasal;
        }

        scanner = savedScanner;
        parser = savedParser;
      }
    }
  }

  return or_(left);
}

static Node *asInterceptor(Node *left) {
  Token opToken = parser.previous;
  int leftArity = (left != NULL && left->type == NODE_TUPLE) ? left->as.tuple.count : 1;

  TrieNode *rootNode = getSignatureTrie("as");
  if (rootNode != NULL) {
    for (int i = 0; i < rootNode->childCount; i++) {
      if (rootNode->children[i]->type == NODE_ARGUMENT &&
          rootNode->children[i]->isLeadingArg &&
          rootNode->children[i]->arity == leftArity) {
        TrieNode *argChild = rootNode->children[i];
        Scanner savedScanner = scanner;
        Parser savedParser = parser;

        if (matchSignatureLookahead(argChild)) {
          Node *phrasal = parsePhrasalInfixCall(argChild, opToken, left);
          if (phrasal != NULL)
            return phrasal;
        }

        scanner = savedScanner;
        parser = savedParser;
      }
    }
  }

  return castExpression(left);
}

static Node *statement();
static Node *block(TokenType *terminators, int count);
static Node *listComprehension(int line) {
  advance(); // Consume the 'for'
  if (check(TOKEN_EACH))
    advance();

  consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
              "I was expecting a variable name here.",
              "Provide a name for your loop iterator.");
  Token iterator = parser.previous;

  Token indexVar;
  bool hasIndex = false;
  if (match(TOKEN_COMMA)) {
    ignoreNewlines();
    consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
                "I was expecting an index variable name.",
                "Provide a name for the index variable after the comma.");
    indexVar = parser.previous;
    hasIndex = true;
  }

  if (check(TOKEN_IN))
    advance();
  else if (check(TOKEN_FROM))
    advance();

  Node *sequence = expression();

  Node *body = NULL;

  if (match(TOKEN_COLON)) {
    Token blockOpener = parser.previous;   // <--- Capture the colon
    TokenType terminators[] = {TOKEN_END}; // <--- Stop at 'end'!
    body = block(terminators, 1);
    consumeBlockEnd(blockOpener,
                    "list comprehension"); // <--- Safely eat the 'end'
  } else {
    body = statement();
  }

  ignoreNewlines();
  consumeHint(TOKEN_RIGHT_BRACKET, ERR_SYNTAX,
              "You opened a block or list here, but forgot to close it.",
              "Make sure to balance your brackets! Add a closing bracket ']' "
              "at the end.");

  return newComprehensionNode(iterator, indexVar, hasIndex, sequence, body,
                              false, line);
}

static Node *dictComprehension(int line) {
  advance(); // Consume the 'for'
  if (check(TOKEN_EACH))
    advance();

  consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
              "I was expecting a variable name here.",
              "Provide a name for your loop iterator.");
  Token iterator = parser.previous;

  Token indexVar;
  bool hasIndex = false;
  if (match(TOKEN_COMMA)) {
    ignoreNewlines();
    consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
                "I was expecting an index variable name.",
                "Provide a name for the index variable after the comma.");
    indexVar = parser.previous;
    hasIndex = true;
  }

  if (check(TOKEN_IN))
    advance();
  else if (check(TOKEN_FROM))
    advance();

  Node *sequence = expression();

  Node *body = NULL;

  if (match(TOKEN_COLON)) {
    Token blockOpener = parser.previous;   // <--- Capture the colon
    TokenType terminators[] = {TOKEN_END}; // <--- Stop at 'end'!
    body = block(terminators, 1);
    consumeBlockEnd(blockOpener,
                    "dictionary comprehension"); // <--- Safely eat the 'end'
  } else {
    body = statement();
  }

  ignoreNewlines();
  consumeHint(
      TOKEN_RIGHT_BRACE, ERR_SYNTAX,
      "You opened a block or dictionary here, but forgot to close it.",
      "Make sure to balance your braces! Add a closing brace '}' at the end.");

  return newComprehensionNode(iterator, indexVar, hasIndex, sequence, body,
                              true, line);
}

static Node *list() {
  int line = parser.previous.line;
  ignoreNewlines();

  if (check(TOKEN_FOR))
    return listComprehension(line);

  NodeArray itemsArr, *items = &itemsArr;
  initNodeArray(items);

  groupingDepth++; // Protect the list items!
  if (!check(TOKEN_RIGHT_BRACKET)) {
    do {
      ignoreNewlines();
      if (check(TOKEN_RIGHT_BRACKET))
        break;
      Node *item = expression();
      validatePureExpression(item, "inside a list");
      writeNodeArray(items, item);
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
  ignoreNewlines();

  if (check(TOKEN_FOR))
    return dictComprehension(line);

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
        char mangled[1024] = {0};
        int currentLen = parser.previous.length;
        strncpy(mangled, parser.previous.start, currentLen);
        mangled[currentLen] = '\0';

        while (check(TOKEN_IDENTIFIER) &&
               !isReservedKeyword(parser.current.type)) {
          advance();
          mangled[currentLen++] = ' ';
          strncpy(mangled + currentLen, parser.previous.start,
                  parser.previous.length);
          currentLen += parser.previous.length;
          mangled[currentLen] = '\0';
        }
        ObjString *keyStr = copyString(mangled, currentLen);
        keyNode = newLiteralNode(OBJ_VAL(keyStr), parser.previous.line);
      } else if (match(TOKEN_STRING)) {
        // If they type `"name":`, chop off the quotes normally
        ObjString *keyStr =
            copyString(parser.previous.start + 1, parser.previous.length - 2);
        keyNode = newLiteralNode(OBJ_VAL(keyStr), parser.previous.line);
      } else if (match(TOKEN_NUMBER)) {
        double value = strtod(parser.previous.start, NULL);
        keyNode = newLiteralNode(NUMBER_VAL(value), parser.previous.line);
      } else {
        errorAt(&parser.previous, ERR_SYNTAX,
                "I was expecting a property name for this dictionary item.",
                "Dictionary keys should be words, strings, or numbers (e.g., "
                "'name:', '\"age\":', or '1:').");
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
        break; // Successfully handles trailing commas!

      // --- THE FIX ---
      if (!check(TOKEN_IDENTIFIER)) {
        errorAt(
            &parser.current, ERR_SYNTAX,
            "I was expecting a property name here.",
            isWith
                ? "When overriding properties, you need to list them "
                  "explicitly."
                : "When instantiating a type, you need to list its properties "
                  "(e.g., 'health: 100').");
      }
      advance();
      char mangled[1024] = {0};
      int currentLen = parser.previous.length;
      strncpy(mangled, parser.previous.start, currentLen);
      mangled[currentLen] = '\0';

      while (check(TOKEN_IDENTIFIER) &&
             !isReservedKeyword(parser.current.type)) {
        advance();
        mangled[currentLen++] = ' ';
        strncpy(mangled + currentLen, parser.previous.start,
                parser.previous.length);
        currentLen += parser.previous.length;
        mangled[currentLen] = '\0';
      }

      Token finalName = parser.previous;
      finalName.start = my_strdup(mangled);
      finalName.length = currentLen;

      writeTokenArray(&propNames, finalName);
      // ---------------

      consumeHint(TOKEN_COLON, ERR_SYNTAX,
                  "I was expecting a colon ':' after the key.",
                  "Dictionary keys and values must be separated by colons.");

      Node *valNode = expression();
      validatePureExpression(valNode, "as a dictionary value");
      writeNodeArray(&values, valNode);
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

  if (parser.current.type == TOKEN_IDENTIFIER) {
    Token rootToken = parser.current;
    char rootWord[256] = {0};
    snprintf(rootWord, sizeof(rootWord), "%.*s", rootToken.length,
             rootToken.start);

    TrieNode *currentNode = getSignatureTrie(rootWord);

    if (currentNode != NULL) {
      Scanner savedScanner = scanner;
      Parser savedParser = parser;
      advance(); // consume the root word

      bool hasLookahead = matchSignatureLookahead(currentNode);

      if (hasLookahead) {
        Node *phrasal = parsePhrasalCall(currentNode, rootToken, true, left);
        if (phrasal != NULL) {
          return phrasal;
        }
      }

      // If we are here, lookahead failed or parsePhrasalCall returned NULL
      scanner = savedScanner;
      parser = savedParser;
    }
  }

  // Fallback
  consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
              "I was expecting a property name after the possessive 's.",
              "Provide the name of the property you want to access (e.g., "
              "'user's age').");
  Token name = parser.previous;
  char propMangled[1024] = {0};
  int propLen = name.length;
  strncpy(propMangled, name.start, propLen);
  propMangled[propLen] = '\0';

  while (check(TOKEN_IDENTIFIER) && !isReservedKeyword(parser.current.type)) {
    advance();
    propMangled[propLen++] = ' ';
    strncpy(propMangled + propLen, parser.previous.start,
            parser.previous.length);
    propLen += parser.previous.length;
    propMangled[propLen] = '\0';
  }

  Token finalProp = name;
  finalProp.start = my_strdup(propMangled);
  finalProp.length = propLen;

  return newPropertyNode(left, finalProp, line);
}

static Node *endKeyword() {
  if (groupingDepth == 0) {
    errorAt(&parser.previous, ERR_SYNTAX, "I wasn't expecting an 'end' here.",
            "Did you accidentally add an extra 'end' to a block that doesn't "
            "need it? Or perhaps you meant to use 'end' inside brackets like "
            "list[1 to end]?");
  }
  return newEndNode(parser.previous.line);
}

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
    Token notToken = {TOKEN_NOT, "not", 3, line, 0, NULL};
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
      Token elseStart = parser.previous;
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
    }
  }

  return newIfNode(condition, thenBranch, elseBranch, line);
}

static Node *whileLogic(bool invert) {
  int line = parser.previous.line;
  Node *condition = expression();

  if (invert) {
    Token notToken = {TOKEN_NOT, "not", 3, line, 0, NULL};
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

  // --- NEW: GRAB THE COMMA! ---
  Token indexVar;
  bool hasIndex = false;
  if (match(TOKEN_COMMA)) {
    consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
                "I was expecting an index/value variable name.",
                "Provide a name for the second variable after the comma.");
    indexVar = parser.previous;
    hasIndex = true;
  }
  // ----------------------------

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

  // Update the constructor call!
  return newForNode(iteratorName, indexVar, hasIndex, sequence, body, line);
}

static Node *parseLValue() {
  // Base Case: The root must be an identifier
  consumeHint(
      TOKEN_IDENTIFIER, ERR_SYNTAX,
      "You started an assignment but didn't tell me what variable to update.",
      "Provide a target variable. For example: 'set score to 10'.");

  Node *lvalue = variable();

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

  if (lvalue != NULL && lvalue->type != NODE_VARIABLE &&
      lvalue->type != NODE_SUBSCRIPT && lvalue->type != NODE_PROPERTY) {
    errorAt(&parser.previous, ERR_SYNTAX, "Invalid assignment target.",
            "You can only assign values to variables, properties, or list items.");
  }

  return lvalue;
}

static Node *addStatement() {
  int line = parser.previous.line;
  NodeArray values;
  initNodeArray(&values);

  // --- THE BLINDFOLD FIX ---
  if (expectedLabelCount >= 256) {
    errorAt(&parser.previous, ERR_SYNTAX,
            "This expression is too deeply nested and I'm losing track of it!",
            "Try breaking this complex calculation into smaller steps using "
            "intermediate variables.");
    return NULL;
  }

  expectedLabelStack[expectedLabelCount].hash = hashString("to", 2);
  expectedLabelStack[expectedLabelCount].depth = groupingDepth;
  expectedLabelStack[expectedLabelCount].labelText = "to";
  expectedLabelStack[expectedLabelCount].labelLength = 2;
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

    // --- THE AST LEAK FIX ---
    // Free the orphaned expressions and the invalid target!
    for (int i = 0; i < values.count; i++)
      freeNode(values.items[i]);
    freeNodeArray(&values);
    freeNode(target);
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
    if ((cond != NULL && cond->usesIt)) {
      Token itToken = makeHiddenToken(" it", line);
      Token *letNames = ALLOCATE(Token, 1);
      letNames[0] = itToken;
      Node **letExprs = ALLOCATE(Node *, 1);
      letExprs[0] = cloneNode(target);
      Node *letIt = newLetNode(letNames, 1, letExprs, 1, line);

      Node *ifStmt = newIfNode(cond, addNode, NULL, line);

      Node **blockStmts = ALLOCATE(Node *, 2);
      blockStmts[0] = letIt;
      blockStmts[1] = ifStmt;

      return newBlockNode(blockStmts, 2, line);
    }
    return newIfNode(cond, addNode, NULL, line);
  } else if (match(TOKEN_UNLESS)) {
    Node *cond = expression();
    Token notToken = {TOKEN_NOT, "not", 3, line, 0, NULL};
    Node *invertedCond = newUnaryNode(notToken, cond, line);

    if ((invertedCond != NULL && invertedCond->usesIt)) {
      Token itToken = makeHiddenToken(" it", line);
      Token *letNames = ALLOCATE(Token, 1);
      letNames[0] = itToken;
      Node **letExprs = ALLOCATE(Node *, 1);
      letExprs[0] = cloneNode(target);
      Node *letIt = newLetNode(letNames, 1, letExprs, 1, line);

      Node *ifStmt = newIfNode(invertedCond, addNode, NULL, line);

      Node **blockStmts = ALLOCATE(Node *, 2);
      blockStmts[0] = letIt;
      blockStmts[1] = ifStmt;

      return newBlockNode(blockStmts, 2, line);
    }
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

    if ((cond != NULL && cond->usesIt)) {
      Token itToken = makeHiddenToken(" it", line);
      Token *letNames = ALLOCATE(Token, 1);
      letNames[0] = itToken;
      Node **letExprs = ALLOCATE(Node *, 1);
      letExprs[0] = cloneNode(targets.items[0]);
      Node *letIt = newLetNode(letNames, 1, letExprs, 1, line);

      Node *ifStmt = newIfNode(cond, setNode, NULL, line);

      Node **blockStmts = ALLOCATE(Node *, 2);
      blockStmts[0] = letIt;
      blockStmts[1] = ifStmt;

      FREE(Node, lastVal);
      freeNodeArray(&targets);
      freeNodeArray(&values);
      return newBlockNode(blockStmts, 2, line);
    }

    FREE(Node, lastVal);
    freeNodeArray(&targets);
    freeNodeArray(&values); // FREE BEFORE RETURN!
    return newIfNode(cond, setNode, NULL, line);
  }

  if (values.count > 1 && values.count != targets.count) {
    errorAt(&parser.previous, ERR_SYNTAX,
            "The number of variables doesn't match the number of values in "
            "this assignment.",
            "Make sure you provide exactly one value for each variable on the "
            "left, or just a single value on the right for all of them.");

    // --- THE PATCH: Destroy the children before the container! ---
    for (int i = 0; i < targets.count; i++)
      freeNode(targets.items[i]);
    for (int i = 0; i < values.count; i++)
      freeNode(values.items[i]);

    freeNodeArray(&targets);
    freeNodeArray(&values);
    return NULL;
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

    // THE GHOST VARIABLE DESUGARING
    if ((cond != NULL && cond->usesIt)) {
      Token itToken = makeHiddenToken(" it", line);
      Token *letNames = ALLOCATE(Token, 1);
      letNames[0] = itToken;
      Node **letExprs = ALLOCATE(Node *, 1);
      letExprs[0] = inner;
      Node *letIt = newLetNode(letNames, 1, letExprs, 1, line);

      Node *itVar = newVariableNode(itToken, line);
      Node *giveStmt = newSingleExprNode(NODE_RETURN, itVar, line);
      Node *ifStmt = newIfNode(cond, giveStmt, NULL, line);

      Node **blockStmts = ALLOCATE(Node *, 2);
      blockStmts[0] = letIt;
      blockStmts[1] = ifStmt;

      FREE(Node, expr);
      return newBlockNode(blockStmts, 2, line);
    }

    Node *giveStmt = newSingleExprNode(NODE_RETURN, inner, line);
    FREE(Node, expr);
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
      if (lastArg != NULL && lastArg->type == NODE_IF &&
          lastArg->as.ifStmt.elseBranch == NULL) {
        Node *cond = lastArg->as.ifStmt.condition;
        expr->as.phrasalCall.arguments[lastIdx] = lastArg->as.ifStmt.thenBranch;
        Node *exprStmt = newSingleExprNode(NODE_EXPRESSION_STMT, expr, line);

        FREE(Node, lastArg); // <--- THE PATCH

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

    FREE(Node, expr);
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
    errorAt(&parser.previous, ERR_SYNTAX,
            "You tried to use 'skip' outside of a loop block.",
            "The 'skip' keyword (which jumps to the next iteration) can only "
            "be used inside 'for' or 'while' loops.");
  }
  return newSkipNode(parser.previous.line);
}

static Node *parsePropertySignatureBody(Token receiverName, Node *receiverType,
                                        int line);
static Node *parseTypeAnnotation();
static bool isReservedKeyword(TokenType type);
static Token makeHiddenToken(const char *text, int line);

static Node *typeDeclaration() {
  int line = parser.previous.line;
  consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
              "You started defining a type but didn't give it a name.",
              "Every type needs a unique name, and I recommend capitalizing "
              "it. (e.g., 'type Player:')");
  Token name = parser.previous;

  if (match(TOKEN_IS)) {
    Node *aliasType = parseTypeAnnotation();
    return newLetNode(&name, 1, &aliasType, 1, line);
  }

  consumeHint(
      TOKEN_COLON, ERR_SYNTAX,
      "This type definition is missing the colon separator.",
      "Type definitions must begin with a colon before listing properties.");

  Token typeStart = parser.previous;

  TokenArray propertyNames;
  initTokenArray(&propertyNames);
  NodeArray defaultValues;
  initNodeArray(&defaultValues);

  NodeArray methods;
  initNodeArray(&methods);

  ignoreNewlines();

  if (!check(TOKEN_END)) {
    while (!check(TOKEN_END) && !check(TOKEN_EOF)) {
      ignoreNewlines();
      if (check(TOKEN_END))
        break; // Allow trailing commas before 'end'

      consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
                  "I was expecting a property or method name here.", "");
      Token rootName = parser.previous;

      TrieNode *currentNode = startPhrase(rootName.start, rootName.length);
      int mangledCap = 1024;
      char *mangled = calloc(1, mangledCap);
      char *mangledField = calloc(1, mangledCap);
      strncat(mangled, rootName.start, rootName.length);
      strncat(mangledField, rootName.start, rootName.length);

      bool lastWasLabel = true;
      bool containsKeyword = false;
      Token firstKeyword;

      TokenArray parameters;
      initTokenArray(&parameters);
      NodeArray paramTypes;
      initNodeArray(&paramTypes);

      while (!check(TOKEN_IS) && !check(TOKEN_COMMA) && !check(TOKEN_COLON) &&
             !check(TOKEN_NEWLINE) && !check(TOKEN_END) && !check(TOKEN_EOF)) {
        if (match(TOKEN_LEFT_PAREN)) {
          if (!lastWasLabel) {
            errorAt(&parser.previous, ERR_SYNTAX,
                    "Sequential arguments are forbidden in signatures.",
                    "Two argument blocks cannot appear consecutively. Use a "
                    "comma-separated tuple instead, like '(a, b)'.");
          }
          lastWasLabel = false;
          int segmentArity = 0;
          if (!check(TOKEN_RIGHT_PAREN)) {
            do {
              consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX, "Expected param name.",
                          "");
              writeTokenArray(&parameters, parser.previous);
              if (match(TOKEN_COLON)) {
                ignoreNewlines();
                writeNodeArray(&paramTypes, parseTypeAnnotation());
              } else {
                Token anyToken = {.type = TOKEN_IDENTIFIER,
                                  .start = "Any",
                                  .length = 3,
                                  .line = parser.previous.line};
                writeNodeArray(&paramTypes,
                               newVariableNode(anyToken, parser.previous.line));
              }
              segmentArity++;
            } while (match(TOKEN_COMMA));
          }
          consumeHint(TOKEN_RIGHT_PAREN, ERR_SYNTAX, "Expected ')'", "");

          char buf[16];
          sprintf(buf, "$%d", segmentArity);
          if ((int)(strlen(mangled) + strlen(buf)) >= mangledCap - 1) {
            mangledCap *= 2;
            mangled = realloc(mangled, mangledCap);
          }
          strcat(mangled, buf);
          currentNode = addArgumentBranch(currentNode, segmentArity, false);
        } else {
          if (!containsKeyword && isReservedKeyword(parser.current.type)) {
            containsKeyword = true;
            firstKeyword = parser.current;
          }
          advance();
          Token labelTok = parser.previous;

          if ((int)(strlen(mangled) + labelTok.length + 5) >= mangledCap) {
            mangledCap *= 2;
            mangled = realloc(mangled, mangledCap);
          }
          if (lastWasLabel) {
            strcat(mangled, "#");
          }
          strncat(mangled, labelTok.start, labelTok.length);

          int fieldLen = strlen(mangledField);
          if (fieldLen + labelTok.length + 5 >= mangledCap) {
            mangledField = realloc(mangledField, mangledCap);
          }
          strcat(mangledField, " ");
          strncat(mangledField, labelTok.start, labelTok.length);

          currentNode =
              addLabelBranch(currentNode, labelTok.start, labelTok.length);
          lastWasLabel = true;
        }
      }

      if (match(TOKEN_COLON)) {
        if (!finalizePhrase(currentNode, mangled, TERMINAL_PHRASE)) {
          errorAt(&parser.previous, ERR_SYNTAX, "Namespace collision.",
                  "A field with this exact name already exists.");
        }

        Token funcStart = parser.previous;
        ignoreNewlines();
        TokenType functionEnds[] = {TOKEN_END};
        Node *body = block(functionEnds, 1);
        consumeBlockEnd(funcStart, "property method declaration");

        Token mangledToken = rootName;
        mangledToken.start = my_strdup(mangled);
        mangledToken.length = strlen(mangled);

        Token receiverName = makeHiddenToken("my", line);
        Node *receiverType = newVariableNode(name, line);

        Node *method = newExtensionMethodNode(
            mangledToken, receiverName, receiverType, parameters.items,
            paramTypes.items, parameters.count, body, line);
        writeNodeArray(&methods, method);

        match(TOKEN_COMMA);
      } else {
        if (parameters.count > 0) {
          errorAt(&parser.previous, ERR_SYNTAX, "Fields cannot have arguments.",
                  "Only methods ending with ':' can have parameters.");
        }
        if (containsKeyword) {
          errorAt(&firstKeyword, ERR_SYNTAX, "Reserved keyword in field name.",
                  "Reserved keywords are forbidden in fields.");
        }

        if (!finalizePhrase(currentNode, mangledField, TERMINAL_VARIABLE)) {
          errorAt(&parser.previous, ERR_SYNTAX, "Namespace collision.",
                  "A method with this exact name already exists.");
        }

        Token fieldName = rootName;
        fieldName.start = my_strdup(mangledField);
        fieldName.length = strlen(mangledField);
        writeTokenArray(&propertyNames, fieldName);

        if (match(TOKEN_IS)) {
          writeNodeArray(&defaultValues, expression());
        } else {
          writeNodeArray(&defaultValues,
                         newLiteralNode(NIL_VAL, parser.previous.line));
        }

        if (!match(TOKEN_COMMA)) {
          ignoreNewlines();
          if (!check(TOKEN_END)) {
            errorAt(&parser.current, ERR_SYNTAX,
                    "Expected ',' between properties.", "");
            break;
          }
        }
      }

      freeTokenArray(&parameters);
      freeNodeArray(&paramTypes);
      free(mangled);
      free(mangledField);
    }
  }

  ignoreNewlines();
  consumeBlockEnd(typeStart, "type definition");

  Node *typeNode = newTypeNode(name, propertyNames.items, defaultValues.items,
                               propertyNames.count, line);
  freeTokenArray(&propertyNames);
  freeNodeArray(&defaultValues);

  if (methods.count > 0) {
    NodeArray stmts;
    initNodeArray(&stmts);
    writeNodeArray(&stmts, typeNode);
    for (int i = 0; i < methods.count; i++) {
      writeNodeArray(&stmts, methods.items[i]);
    }
    Node *blockNode = newBlockNode(stmts.items, stmts.count, line);
    freeNodeArray(&stmts);
    freeNodeArray(&methods);
    return blockNode;
  }

  freeNodeArray(&methods);
  return typeNode;
}

// Helper to create our invisible ghost tokens
static bool isReservedKeyword(TokenType type);
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
      Node *originalIf = mathValue; // <--- Save the pointer!
      mathValue = mathValue->as.ifStmt.thenBranch;
      FREE(Node, originalIf); // <--- THE PATCH
    }
  } else {
    errorAt(&parser.current, ERR_SYNTAX,
            "I was expecting 'as' or a math operator here.",
            "The update statement requires 'as' (to cast) or a math operator "
            "(+, -, *, /, %). e.g., 'update x as List' or 'update x * 2'.");
    FREE(Node, rawTarget);
    return NULL;
  }

  // 3. THE DESUGARING (Building the Universal RHS)
  Node *finalNode = NULL;

  // --- PREVENT USE-AFTER-FREE ---
  // We must clone the target before desugaring, because complex
  // targets (like NODE_PROPERTY) will be explicitly freed below!
  Node *rawTargetBackup = cloneNode(rawTarget);

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

  // --- 4. RE-WRAP THE AST & APPLY GHOST VARIABLES ---
  if (modifierCond != NULL) {
    if ((modifierCond != NULL && modifierCond->usesIt)) {
      Token itToken = makeHiddenToken(" it", line);
      Token *letNames = ALLOCATE(Token, 1);
      letNames[0] = itToken;
      Node **letExprs = ALLOCATE(Node *, 1);
      letExprs[0] = rawTargetBackup; // The L-Value!
      Node *letIt = newLetNode(letNames, 1, letExprs, 1, line);

      Node *ifStmt = newIfNode(modifierCond, finalNode, NULL, line);

      Node **blockStmts = ALLOCATE(Node *, 2);
      blockStmts[0] = letIt;
      blockStmts[1] = ifStmt;

      return newBlockNode(blockStmts, 2, line);
    }
    FREE(Node, rawTargetBackup);
    return newIfNode(modifierCond, finalNode, NULL, line);
  }

  // --- OPTIONAL: AST INVERSION (Statement Modifiers) ---
  if (match(TOKEN_IF)) {
    Node *cond = expression();
    if ((cond != NULL && cond->usesIt)) {
      Token itToken = makeHiddenToken(" it", line);
      Token *letNames = ALLOCATE(Token, 1);
      letNames[0] = itToken;
      Node **letExprs = ALLOCATE(Node *, 1);
      letExprs[0] = rawTargetBackup;
      Node *letIt = newLetNode(letNames, 1, letExprs, 1, line);

      Node *ifStmt = newIfNode(cond, finalNode, NULL, line);

      Node **blockStmts = ALLOCATE(Node *, 2);
      blockStmts[0] = letIt;
      blockStmts[1] = ifStmt;

      return newBlockNode(blockStmts, 2, line);
    }
    FREE(Node, rawTargetBackup);
    return newIfNode(cond, finalNode, NULL, line);
  } else if (match(TOKEN_UNLESS)) {
    Node *cond = expression();
    Token notToken = {TOKEN_NOT, "not", 3, line, 0, NULL};
    Node *invertedCond = newUnaryNode(notToken, cond, line);

    if ((invertedCond != NULL && invertedCond->usesIt)) {
      Token itToken = makeHiddenToken(" it", line);
      Token *letNames = ALLOCATE(Token, 1);
      letNames[0] = itToken;
      Node **letExprs = ALLOCATE(Node *, 1);
      letExprs[0] = rawTargetBackup;
      Node *letIt = newLetNode(letNames, 1, letExprs, 1, line);

      Node *ifStmt = newIfNode(invertedCond, finalNode, NULL, line);

      Node **blockStmts = ALLOCATE(Node *, 2);
      blockStmts[0] = letIt;
      blockStmts[1] = ifStmt;

      return newBlockNode(blockStmts, 2, line);
    }
    FREE(Node, rawTargetBackup);
    return newIfNode(invertedCond, finalNode, NULL, line);
  }

  FREE(Node, rawTargetBackup);
  return finalNode;
}

extern char *readFile(const char *path);

#define MAX_VISITED_FILES 128
static const char *visitedFiles[MAX_VISITED_FILES];
static int visitedCount = 0;

static Node *loadStatement() {
  int line = parser.previous.line;
  consumeHint(TOKEN_STRING, ERR_SYNTAX, "I was expecting a file path here.",
              "The load statement requires a file path in quotes (e.g., load "
              "\"math.moon\" as math).");
  Token pathToken = parser.previous;

  consumeHint(TOKEN_AS, ERR_SYNTAX, "I was expecting 'as' here.",
              "The load statement requires an alias (e.g., load \"math.moon\" "
              "as math).");

  consumeHint(
      TOKEN_IDENTIFIER, ERR_SYNTAX, "I was expecting a variable name here.",
      "The load statement needs an identifier to assign the module to.");
  Token aliasToken = parser.previous;

  Node *loadExpr = newLoadNode(pathToken, line);

  // Desugar into 'let aliasToken be loadExpr'
  Token names[1] = {aliasToken};
  Node *exprs[1] = {loadExpr};
  Node *node = newLetNode(names, 1, exprs, 1, line);

  // --- COMPILE-TIME METADATA SCANNER ---
  char pathStr[256] = {0};
  snprintf(pathStr, sizeof(pathStr), "%.*s", pathToken.length - 2,
           pathToken.start + 1);

  bool alreadyVisited = false;
  for (int i = 0; i < visitedCount; i++) {
    if (strcmp(visitedFiles[i], pathStr) == 0) {
      alreadyVisited = true;
      break;
    }
  }

  if (!alreadyVisited && visitedCount < MAX_VISITED_FILES) {
    visitedFiles[visitedCount] = my_strdup(pathStr);
    visitedCount++;

    char *source = readFile(pathStr);
    if (source != NULL) {
      Scanner savedScanner = scanner;
      Parser savedParser = parser;

      extern Node *parseSource(const char *source, int startLine);
      Node *discardAst = parseSource(source, 1);
      if (discardAst != NULL)
        freeNode(discardAst);

      free(source);

      scanner = savedScanner;
      parser = savedParser;
    }
  }

  return node;
}

static Node *keepStatement() {
  int line = parser.previous.line;
  Node *key = NULL;
  Node *value = expression();

  // If there is a colon, it's a dictionary keep! (keep key : value)
  if (match(TOKEN_COLON)) {
    key = value; // The first expression was actually the key!
    value = expression();
  }

  // --- AST INVERSION (Statement Modifiers) ---
  // Because the modifier is always at the end of the line,
  // the Pratt parser attaches it to the 'value' expression.
  if (value != NULL && value->type == NODE_IF &&
      value->as.ifStmt.elseBranch == NULL) {
    Node *cond = value->as.ifStmt.condition;
    Node *innerValue = value->as.ifStmt.thenBranch;

    // THE GHOST VARIABLE DESUGARING
    if ((cond != NULL && cond->usesIt)) {
      Token itToken = makeHiddenToken(" it", line);
      Token *letNames = ALLOCATE(Token, 1);
      letNames[0] = itToken;
      Node **letExprs = ALLOCATE(Node *, 1);
      letExprs[0] = innerValue;
      Node *letIt = newLetNode(letNames, 1, letExprs, 1, line);

      Node *itVar = newVariableNode(itToken, line);
      Node *keepNode = newKeepNode(key, itVar, line);
      Node *ifStmt = newIfNode(cond, keepNode, NULL, line);

      Node **blockStmts = ALLOCATE(Node *, 2);
      blockStmts[0] = letIt;
      blockStmts[1] = ifStmt;

      FREE(Node, value);
      return newBlockNode(blockStmts, 2, line);
    }

    // 1. Rebuild the keep node with the raw inner value
    Node *keepNode = newKeepNode(key, innerValue, line);

    FREE(Node, value);
    // 2. Wrap the keep node completely inside the IF block!
    return newIfNode(cond, keepNode, NULL, line);
  }

  return newKeepNode(key, value, line);
}

static Node *statement() {

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
  } else if (match(TOKEN_KEEP)) {
    stmt = keepStatement();
  } else {
    stmt = expressionStatement();
  }

  // --- 3. THE GLOBAL MODIFIER CATCH ---
  // Catches trailing modifiers for break, skip, quit, and load!
  if (match(TOKEN_IF)) {
    Node *cond = expression();
    stmt = newIfNode(cond, stmt, NULL, parser.previous.line);
  } else if (match(TOKEN_UNLESS)) {
    Node *cond = expression();
    Token notToken = {TOKEN_NOT, "not", 3, parser.previous.line, 0, NULL};
    Node *invertedCond = newUnaryNode(notToken, cond, parser.previous.line);
    stmt = newIfNode(invertedCond, stmt, NULL, parser.previous.line);
  }

  return stmt;
}

static Node *parseTypeAnnotation() {
  int line = parser.previous.line;

  // 1. We expect at least one Type name (e.g. 'String')
  consumeHint(TOKEN_IDENTIFIER, ERR_TYPE, "I was expecting a type name here.",
              "Type annotations must specify a valid blueprint type (e.g., "
              "'name: String').");
  Node *baseType = newVariableNode(parser.previous, line);

  // 2. Is it a union? Look ahead for the 'or' keyword!
  if (check(TOKEN_OR)) {
    NodeArray types;
    initNodeArray(&types);
    writeNodeArray(&types, baseType);

    while (match(TOKEN_OR)) {
      consumeHint(TOKEN_IDENTIFIER, ERR_TYPE,
                  "You created a Union type with 'or' but didn't provide the "
                  "right-hand type.",
                  "Unions must link valid types. (e.g., 'String or Number')");
      writeNodeArray(&types,
                     newVariableNode(parser.previous, parser.previous.line));
    }

    Node *unionNode = newUnionTypeNode(types.items, types.count, line);
    freeNodeArray(&types);
    return unionNode;
  }

  return baseType; // Just a normal, single type
}

static bool isReservedKeyword(TokenType type) {
  if (type >= TOKEN_AND && type <= TOKEN_WITH)
    return true;
  if (type == TOKEN_IS || type == TOKEN_TO || type == TOKEN_BE)
    return true;
  if (type == TOKEN_LET || type == TOKEN_IF || type == TOKEN_ELSE)
    return true;
  if (type == TOKEN_FOR || type == TOKEN_WHILE)
    return true;
  return false;
}

static bool registerVariableInTrie(const char *varName, int len) {
  const char *start = varName;
  const char *end = varName + len;
  const char *p = start;
  while (p < end && *p != ' ')
    p++;

  TrieNode *node = startPhrase(start, (int)(p - start));
  while (p < end) {
    while (p < end && *p == ' ')
      p++;
    if (p >= end)
      break;
    const char *wordStart = p;
    while (p < end && *p != ' ')
      p++;
    node = addLabelBranch(node, wordStart, (int)(p - wordStart));
  }
  return finalizePhrase(node, varName, TERMINAL_VARIABLE);
}

static Node *letDeclaration() {
  int line = parser.previous.line;

  TokenArray names;
  initTokenArray(&names);

  TokenArray parameters;
  initTokenArray(&parameters);
  NodeArray paramTypes;
  initNodeArray(&paramTypes);

  Token rootName;
  int leadingArity = 0;
  bool startedWithArg = false;
  bool hasCustomAnchor = false;
  bool rootIsOperatorOrKeyword = false;

  int mangledCap = 1024;
  char *mangled = calloc(1, mangledCap);
  char *mangledVar = calloc(1, mangledCap);

  if (check(TOKEN_LEFT_PAREN)) {
    startedWithArg = true;
    advance(); // consume '('

    Token receiverName;
    Node *receiverType = NULL;

    if (check(TOKEN_RIGHT_PAREN)) {
      errorAt(&parser.current, ERR_SYNTAX,
              "Empty parentheses '()' are not allowed.",
              "If a function takes no arguments, just write its name without "
              "parentheses.");
    } else {
      do {
        consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
                    "You opened a parameter definition but forgot to name the "
                    "parameter.",
                    "Give the parameter a name inside the parentheses. (e.g., "
                    "'(age: Number)')");
        writeTokenArray(&parameters, parser.previous);
        if (leadingArity == 0)
          receiverName = parser.previous;

        if (match(TOKEN_COLON)) {
          ignoreNewlines();
          Node *t = parseTypeAnnotation();
          writeNodeArray(&paramTypes, t);
          if (leadingArity == 0)
            receiverType = t;
        } else {
          Token anyToken = {.type = TOKEN_IDENTIFIER,
                            .start = "Any",
                            .length = 3,
                            .line = parser.previous.line};
          Node *t = newVariableNode(anyToken, parser.previous.line);
          writeNodeArray(&paramTypes, t);
          if (leadingArity == 0)
            receiverType = t;
        }
        leadingArity++;
      } while (match(TOKEN_COMMA));
    }

    consumeHint(TOKEN_RIGHT_PAREN, ERR_SYNTAX,
                "You opened a parameter list, but forgot to close it.",
                "Make sure to balance your parentheses! Add a closing ')' at "
                "the end.");

    // Check for extension method: let (p: Player)'s ...
    if (match(TOKEN_POSSESSIVE)) {
      free(mangled);
      free(mangledVar);
      freeTokenArray(&names);
      freeTokenArray(&parameters);
      freeNodeArray(&paramTypes);
      return parsePropertySignatureBody(receiverName, receiverType, line);
    }

    // Check if next token is another argument (sequential arguments error)
    if (check(TOKEN_LEFT_PAREN)) {
      errorAt(&parser.current, ERR_SYNTAX,
              "Sequential arguments are forbidden in signatures.",
              "Two argument blocks cannot appear consecutively. Use a "
              "comma-separated tuple instead, like '(a, b)'.");
      free(mangled);
      free(mangledVar);
      freeTokenArray(&names);
      freeTokenArray(&parameters);
      freeNodeArray(&paramTypes);
      return NULL;
    }

    // Argument-led phrase: next token is root word/operator
    if (check(TOKEN_EOF) || check(TOKEN_NEWLINE) || check(TOKEN_COLON) ||
        check(TOKEN_BE)) {
      errorAt(&parser.current, ERR_SYNTAX,
              "Expected phrasal root word or operator after leading argument.",
              "e.g. 'let (s) split by (sep):' or 'let (a) + (b) days:'");
      free(mangled);
      free(mangledVar);
      freeTokenArray(&names);
      freeTokenArray(&parameters);
      freeNodeArray(&paramTypes);
      return NULL;
    }

    advance();
    rootName = parser.previous;
    writeTokenArray(&names, rootName);

    if (rootName.type == TOKEN_IDENTIFIER &&
        !isReservedKeyword(rootName.type)) {
      hasCustomAnchor = true;
    } else {
      rootIsOperatorOrKeyword = true;
    }

    strncat(mangled, rootName.start, rootName.length);
    char buf[16];
    sprintf(buf, "$%d", leadingArity);
    strcat(mangled, buf);

  } else if (match(TOKEN_IDENTIFIER)) {
    rootName = parser.previous;
    writeTokenArray(&names, rootName);
    strncat(mangled, rootName.start, rootName.length);
    strncat(mangledVar, rootName.start, rootName.length);
    if (!isReservedKeyword(rootName.type)) {
      hasCustomAnchor = true;
    }
  } else {
    errorAt(&parser.current, ERR_SYNTAX, "I was expecting a name here.",
            "Variables or functions need a name to identify them. e.g., 'let "
            "count be 10'");
    free(mangled);
    free(mangledVar);
    freeTokenArray(&names);
    freeTokenArray(&parameters);
    freeNodeArray(&paramTypes);
    return NULL;
  }

  TrieNode *currentNode = startPhrase(rootName.start, rootName.length);
  if (startedWithArg) {
    currentNode = addArgumentBranch(currentNode, leadingArity, true);
  }

  bool lastWasLabel = true;
  bool containsKeyword = isReservedKeyword(rootName.type);
  Token firstKeyword = rootName;

  while (!check(TOKEN_BE) && !check(TOKEN_EQUAL) && !check(TOKEN_COMMA) &&
         !check(TOKEN_COLON) && !check(TOKEN_EOF) && !check(TOKEN_NEWLINE)) {

    if (match(TOKEN_LEFT_PAREN)) {
      if (!lastWasLabel) {
        errorAt(&parser.previous, ERR_SYNTAX,
                "Sequential arguments are forbidden in signatures.",
                "Two argument blocks cannot appear consecutively. Use a "
                "comma-separated tuple instead, like '(a, b)'.");
      }
      lastWasLabel = false;

      if (check(TOKEN_RIGHT_PAREN)) {
        errorAt(&parser.current, ERR_SYNTAX,
                "Empty parentheses '()' are not allowed.",
                "If a function takes no arguments, just write its name without "
                "parentheses (e.g., 'let jump:').");
      }

      int segmentArity = 0;
      if (!check(TOKEN_RIGHT_PAREN)) {
        do {
          consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
                      "You opened a parameter definition but forgot to "
                      "name the parameter.",
                      "Give the parameter a name inside the parentheses. "
                      "(e.g., '(age: Number)')");
          writeTokenArray(&parameters, parser.previous);

          if (match(TOKEN_COLON)) {
            ignoreNewlines();
            writeNodeArray(&paramTypes, parseTypeAnnotation());
          } else {
            Token anyToken = {.type = TOKEN_IDENTIFIER,
                              .start = "Any",
                              .length = 3,
                              .line = parser.previous.line};
            writeNodeArray(&paramTypes,
                           newVariableNode(anyToken, parser.previous.line));
          }
          segmentArity++;
        } while (match(TOKEN_COMMA));
      }

      consumeHint(TOKEN_RIGHT_PAREN, ERR_SYNTAX,
                  "You opened a parameter list, but forgot to close it.",
                  "Make sure to balance your parentheses! Add a closing "
                  "')' at the end.");

      char buf[16];
      sprintf(buf, "$%d", segmentArity);
      int currentLen = strlen(mangled);
      if ((int)(currentLen + strlen(buf)) >= mangledCap - 1) {
        mangledCap *= 2;
        mangled = realloc(mangled, mangledCap);
      }
      strcat(mangled, buf);
      currentNode = addArgumentBranch(currentNode, segmentArity, false);
    } else {
      if (!containsKeyword && isReservedKeyword(parser.current.type)) {
        containsKeyword = true;
        firstKeyword = parser.current;
      }
      if (parser.current.type == TOKEN_IDENTIFIER &&
          !isReservedKeyword(parser.current.type)) {
        hasCustomAnchor = true;
      }
      advance();
      Token labelTok = parser.previous;

      int currentLen = strlen(mangled);
      if (currentLen + labelTok.length + 5 >= mangledCap) {
        mangledCap *= 2;
        mangled = realloc(mangled, mangledCap);
      }
      if (lastWasLabel) {
        strcat(mangled, "#");
      }
      strncat(mangled, labelTok.start, labelTok.length);

      int varLen = strlen(mangledVar);
      if (varLen + labelTok.length + 5 >= mangledCap) {
        mangledVar = realloc(mangledVar, mangledCap);
      }
      strcat(mangledVar, " ");
      strncat(mangledVar, labelTok.start, labelTok.length);

      currentNode =
          addLabelBranch(currentNode, labelTok.start, labelTok.length);
      lastWasLabel = true;
    }
  }

  // Update the first variable name to its full spaced representation
  names.items[0].start = my_strdup(mangledVar);
  names.items[0].length = strlen(mangledVar);

  if (check(TOKEN_COMMA)) {
    if (parameters.count > 0 || startedWithArg) {
      errorAt(&parser.current, ERR_SYNTAX,
              "Invalid comma in phrasal declaration.",
              "You cannot declare multiple phrases or functions in a single "
              "line.");
    }
    while (match(TOKEN_COMMA)) {
      if (check(TOKEN_LEFT_PAREN)) {
        errorAt(&parser.current, ERR_SYNTAX,
                "Variables cannot have arguments.",
                "You used parentheses in a variable declaration list.");
        advance();
        break;
      }
      if (isReservedKeyword(parser.current.type)) {
        errorAt(&parser.current, ERR_SYNTAX,
                "Reserved keyword used inside variable name.",
                "You cannot use reserved keywords (like 'and', 'or', 'is') inside "
                "spaced variable names. They are only allowed in phrasal functions.");
        advance();
        break;
      }
      consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
                  "I was expecting a variable name here.",
                  "e.g., 'let a, b be 1, 2'");
      Token subRoot = parser.previous;
      int subCap = 128;
      char *subVar = calloc(1, subCap);
      if ((int)subRoot.length + 1 >= subCap) {
        subCap = subRoot.length + 16;
        subVar = realloc(subVar, subCap);
      }
      strncat(subVar, subRoot.start, subRoot.length);

      while (!check(TOKEN_BE) && !check(TOKEN_EQUAL) && !check(TOKEN_COMMA) &&
             !check(TOKEN_COLON) && !check(TOKEN_EOF) && !check(TOKEN_NEWLINE)) {
        if (check(TOKEN_LEFT_PAREN)) {
          errorAt(&parser.current, ERR_SYNTAX,
                  "Variables cannot have arguments.",
                  "You used parentheses in a variable declaration list.");
          advance();
          break;
        }
        if (isReservedKeyword(parser.current.type)) {
          errorAt(&parser.current, ERR_SYNTAX,
                  "Reserved keyword used inside variable name.",
                  "You cannot use reserved keywords (like 'and', 'or', 'is') inside "
                  "spaced variable names. They are only allowed in phrasal functions.");
          advance();
          break;
        }
        if (!check(TOKEN_IDENTIFIER)) {
          errorAt(&parser.current, ERR_SYNTAX,
                  "I was expecting a variable name here.",
                  "e.g., 'let first name, last name be ...'");
          advance();
          break;
        }

        advance();
        Token labelTok = parser.previous;
        int curLen = strlen(subVar);
        if (curLen + labelTok.length + 5 >= subCap) {
          subCap *= 2;
          subVar = realloc(subVar, subCap);
        }
        strcat(subVar, " ");
        strncat(subVar, labelTok.start, labelTok.length);
      }

      Token nextVarTok = {
          .type = TOKEN_IDENTIFIER,
          .start = my_strdup(subVar),
          .length = strlen(subVar),
          .line = subRoot.line,
      };
      writeTokenArray(&names, nextVarTok);
      free(subVar);
    }
  }

  if (match(TOKEN_BE) || match(TOKEN_EQUAL)) {
    if (parser.previous.type == TOKEN_EQUAL) {
      errorAt(&parser.previous, ERR_SYNTAX,
              "It looks like you used '=' to assign a variable.",
              "In MOON, we use the word 'be' for new variables. Try changing "
              "'=' to 'be' (e.g., 'let x be 10').");
    }

    if (parameters.count > 0 || startedWithArg) {
      errorAt(&parser.previous, ERR_SYNTAX, "Variables cannot have arguments.",
              "You used parentheses, but ended with 'be'. Phrases must end "
              "with ':'.");
    }

    if (containsKeyword) {
      errorAt(
          &firstKeyword, ERR_SYNTAX,
          "Reserved keyword used inside variable name.",
          "You cannot use reserved keywords (like 'and', 'or', 'is') inside "
          "spaced variable names. They are only allowed in phrasal functions.");
    }

    for (int i = 0; i < names.count; i++) {
      char singleMangled[256];
      snprintf(singleMangled, sizeof(singleMangled), "%.*s",
               names.items[i].length, names.items[i].start);
      if (!registerVariableInTrie(singleMangled, names.items[i].length)) {
        errorAt(&parser.previous, ERR_SYNTAX, "Namespace collision.",
                "A phrase with this exact signature already exists.");
      }
    }

    NodeArray exprs;
    initNodeArray(&exprs);
    do {
      Node *valNode = expression();
      validatePureExpression(valNode, "as a variable assignment");
      writeNodeArray(&exprs, valNode);
    } while (match(TOKEN_COMMA));

    if (exprs.count != 1 && exprs.count != names.count) {
      errorAt(
          &parser.previous, ERR_SYNTAX, "Mismatch in assignment counts.",
          "When declaring multiple variables, you must provide exactly 1 value "
          "(to copy to all), or exactly match the number of variables.");
      for (int i = 0; i < exprs.count; i++)
        freeNode(exprs.items[i]);
      freeNodeArray(&exprs);
      freeTokenArray(&names);
      freeTokenArray(&parameters);
      freeNodeArray(&paramTypes);
      free(mangled);
      free(mangledVar);
      return NULL;
    }

    Node *node =
        newLetNode(names.items, names.count, exprs.items, exprs.count, line);
    freeNodeArray(&exprs);
    freeTokenArray(&names);
    freeTokenArray(&parameters);
    freeNodeArray(&paramTypes);
    free(mangled);
    free(mangledVar);
    return node;
  } else if (match(TOKEN_COLON)) {
    if (names.count > 1) {
      errorAt(&parser.previous, ERR_SYNTAX, "Multiple function declarations.",
              "Function declarations must happen one at a time.");
    }

    if (rootIsOperatorOrKeyword && !hasCustomAnchor) {
      errorAt(
          &parser.previous, ERR_SYNTAX,
          "Operator phrase requires a custom identifier anchor.",
          "Phrases starting with operators or keywords (like '+', 'is', 'and') "
          "must contain at least one non-reserved identifier (e.g., 'let (a) + "
          "(b) days:').");
    }

    if (!finalizePhrase(currentNode, mangled, TERMINAL_PHRASE)) {
      errorAt(&parser.previous, ERR_SYNTAX, "Namespace collision.",
              "A variable with this exact name already exists.");
    }

    Token funcStart = parser.previous;
    TokenType terminators[] = {TOKEN_END};
    Node *body = block(terminators, 1);
    consumeBlockEnd(funcStart, "function");

    Token finalName = rootName;
    finalName.start = my_strdup(mangled);
    finalName.length = strlen(mangled);

    Node *node = newFunctionNode(finalName, parameters.items, paramTypes.items,
                                 parameters.count, body, line);
    freeTokenArray(&parameters);
    freeNodeArray(&paramTypes);
    freeTokenArray(&names);
    free(mangled);
    free(mangledVar);
    return node;
  }

  errorAt(&parser.previous, ERR_SYNTAX,
          "I'm totally lost trying to read this declaration.",
          "Expected 'be' or ':'.");
  freeTokenArray(&names);
  freeTokenArray(&parameters);
  freeNodeArray(&paramTypes);
  free(mangled);
  free(mangledVar);
  return NULL;
}

static Node *grouping() {
  groupingDepth++; // Going deeper...

  NodeArray elements;
  initNodeArray(&elements);

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      Node *exprNode = expression();
      validatePureExpression(exprNode, "inside parentheses");
      writeNodeArray(&elements, exprNode);
    } while (match(TOKEN_COMMA));
  }

  groupingDepth--; // Coming back out!

  consumeHint(TOKEN_RIGHT_PAREN, ERR_SYNTAX,
              "You opened a parameter list, but forgot to close it.",
              "Make sure you close any opened parentheses in your math, logic, "
              "or function calls.");

  if (elements.count == 1) {
    Node *expr = elements.items[0];
    freeNodeArray(&elements);
    return newSingleExprNode(NODE_GROUPING, expr, parser.previous.line);
  } else {
    Node *tuple =
        newTupleNode(elements.items, elements.count, parser.previous.line);
    freeNodeArray(&elements);
    return tuple;
  }
}



static Node *parsePropertySignatureBody(Token receiverName, Node *receiverType,
                                        int line) {
  consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX, "Expected property name.", "");
  Token rootName = parser.previous;

  TokenArray parameters;
  initTokenArray(&parameters);
  NodeArray paramTypes;
  initNodeArray(&paramTypes);

  char mangled[1024] = {0};
  strncat(mangled, rootName.start, rootName.length);

  TrieNode *currentNode = startPhrase(rootName.start, rootName.length);

  if (check(TOKEN_COLON)) {
    if (!finalizePhrase(currentNode, mangled, TERMINAL_PHRASE)) {
      errorAt(&parser.previous, ERR_SYNTAX, "Namespace collision.",
              "A property with this exact name already exists.");
    }
  } else {
    bool lastWasLabel = true;

    while (!check(TOKEN_COLON) && !check(TOKEN_EOF) && !check(TOKEN_NEWLINE)) {
      if (match(TOKEN_LEFT_PAREN)) {
        if (!lastWasLabel) {
          errorAt(&parser.previous, ERR_SYNTAX,
                  "Sequential arguments are forbidden in signatures.",
                  "Two argument blocks cannot appear consecutively. Use a "
                  "comma-separated tuple instead, like '(a, b)'.");
        }
        lastWasLabel = false;
        int segmentArity = 0;
        if (!check(TOKEN_RIGHT_PAREN)) {
          do {
            consumeHint(TOKEN_IDENTIFIER, ERR_SYNTAX,
                        "You opened a parameter definition but forgot to name "
                        "the parameter.",
                        "Give the parameter a name inside the parentheses.");
            writeTokenArray(&parameters, parser.previous);

            if (match(TOKEN_COLON)) {
              ignoreNewlines();
              writeNodeArray(&paramTypes, parseTypeAnnotation());
            } else {
              Token anyToken = {.type = TOKEN_IDENTIFIER,
                                .start = "Any",
                                .length = 3,
                                .line = parser.previous.line};
              writeNodeArray(&paramTypes,
                             newVariableNode(anyToken, parser.previous.line));
            }
            segmentArity++;
          } while (match(TOKEN_COMMA));
        }
        consumeHint(TOKEN_RIGHT_PAREN, ERR_SYNTAX, "Expected ')'", "");

        char arityStr[32];
        snprintf(arityStr, sizeof(arityStr), "$%d", segmentArity);
        strcat(mangled, arityStr);

        currentNode = addArgumentBranch(currentNode, segmentArity, false);
      } else {
        advance();
        if (lastWasLabel) {
          strcat(mangled, "#");
        }
        strncat(mangled, parser.previous.start, parser.previous.length);

        currentNode = addLabelBranch(currentNode, parser.previous.start,
                                     parser.previous.length);
        lastWasLabel = true;
      }
    }

    if (!finalizePhrase(currentNode, mangled, TERMINAL_PHRASE)) {
      errorAt(&parser.previous, ERR_SYNTAX, "Namespace collision.",
              "A property with this exact name already exists.");
    }
  }

  consumeHint(TOKEN_COLON, ERR_SYNTAX,
              "Expected ':' to start property method body.", "");
  Token blockOpener = parser.previous;
  ignoreNewlines();

  TokenType functionEnds[] = {TOKEN_END};
  Node *body = block(functionEnds, 1);
  consumeBlockEnd(blockOpener, "property method declaration");

  Token mangledToken = rootName;
  mangledToken.start = my_strdup(mangled);
  mangledToken.length = strlen(mangled);

  Node *node = newExtensionMethodNode(mangledToken, receiverName, receiverType,
                                      parameters.items, paramTypes.items,
                                      parameters.count, body, line);

  freeTokenArray(&parameters);
  freeNodeArray(&paramTypes);
  return node;
}

static Node *declaration() {
  ignoreNewlines();
  if (check(TOKEN_EOF))
    return NULL;

  Node *decl;
  if (match(TOKEN_TYPE)) {
    decl = typeDeclaration();
  } else if (match(TOKEN_LET)) {
    decl = letDeclaration();
  } else {
    decl = statement();
  }

  if (!parser.hadError) {
    consumeStatementEnd();
  }

  if (parser.panicMode)
    synchronize();
  return decl;
}

// ==========================================
// 7. RULES TABLE & ENTRY
// ==========================================

ParseRule rules[] = {
    [TOKEN_AS] = {NULL, asInterceptor, PREC_CAST},
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {dict, instantiate, PREC_CALL}, // <--- Updated!
    [TOKEN_WITH] = {NULL, instantiateWith, PREC_CALL},   // <--- New!
    [TOKEN_MINUS] = {unary, binaryInterceptor, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binaryInterceptor, PREC_TERM},
    [TOKEN_SLASH] = {NULL, binaryInterceptor, PREC_FACTOR},
    [TOKEN_STAR] = {explicitSticky, binaryInterceptor, PREC_FACTOR},
    [TOKEN_MOD] = {NULL, binaryInterceptor, PREC_FACTOR},
    [TOKEN_IS] = {stickyPrefix, isInterceptor, PREC_COMPARISON},
    [TOKEN_EQUAL_EQUAL] = {stickyPrefix, binaryInterceptor, PREC_COMPARISON},
    [TOKEN_EQUAL] = {stickyPrefix, binaryInterceptor, PREC_COMPARISON},
    [TOKEN_GREATER] = {stickyPrefix, binaryInterceptor, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {stickyPrefix, binaryInterceptor, PREC_COMPARISON},
    [TOKEN_LESS] = {stickyPrefix, binaryInterceptor, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {stickyPrefix, binaryInterceptor, PREC_COMPARISON},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_STRING_OPEN] = {interpolation, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_IDENTIFIER] = {variable, phrasalInfix, PREC_PHRASE},
    [TOKEN_IT] = {implicitIt, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_UNLESS] = {NULL, NULL, PREC_NONE},
    [TOKEN_NOT] = {unary, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, andInterceptor, PREC_AND},
    [TOKEN_OR] = {NULL, orInterceptor, PREC_OR},
    [TOKEN_TO] = {NULL, range, PREC_RANGE},
    [TOKEN_LEFT_BRACKET] = {list, subscript, PREC_CALL},
    [TOKEN_DOT] = {NULL, dot, PREC_CALL},
    [TOKEN_POSSESSIVE] = {NULL, possessive, PREC_CALL},
    [TOKEN_END] = {endKeyword, NULL, PREC_NONE},

    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *getRule(TokenType type) { return &rules[type]; }

// --- THE LSP STATE PURGE ---
// Wipes all global tracking variables clean before a new parse run!
static void resetParserState() {
  expectedLabelCount = 0;
  groupingDepth = 0;

  loopingDepth = 0;
  parseDepth = 0;

  // ONLY wipe long-term memory if we are running in the Language Server!
  if (isLspMode) {

    // Destroy the old Signature Trie so deleted/edited functions don't haunt
    // the parser!
    freeSignatureTable();
  }
}

Node *parseSource(const char *source, int startLine) {
  initScanner(source, startLine);
  resetParserState();

  parser.hadError = false;
  parser.panicMode = false;
  advance();

  NodeArray statements;
  initNodeArray(&statements);

  while (!match(TOKEN_EOF)) {
    Node *decl = declaration();
    if (decl != NULL) {
      writeNodeArray(&statements, decl);
    }
  }

  // --- THE LSP AST RESCUE FIX ---
  // Even if parser.hadError is true, we ALWAYS return the block node.
  // The MOON compiler (in compiler.c) already checks parser.hadError
  // and will safely abort compilation, but now the LSP will be able
  // to read the partial tree and salvage the variables!
  Node *node = newBlockNode(statements.items, statements.count, 0);

  freeNodeArray(&statements);
  return node;
}
