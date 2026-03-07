#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "emitter.h"
#include "memory.h"
#include "object.h"
#include "parser.h"
#include "scout.h"

// ==========================================
// PRATT PARSER DEFINITIONS
// ==========================================

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_RANGE,      // to
  PREC_OR,         // or
  PREC_AND,        // and
  PREC_EQUALITY,   // == != is
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! - not
  PREC_CALL,       // . () []
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)();
typedef void (*StatementFn)();

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

// Forward declarations
static void expression();
static void statement();
static void declaration();
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

// ==========================================
// VARIABLES & IDENTIFIERS
// ==========================================

static uint8_t identifierConstant(Token *name) {
  return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static uint8_t parseVariableConstant(const char *errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);
  return identifierConstant(&parser.previous);
}

static uint8_t parseVariable(const char *errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);
  declareVariable();
  if (current->scopeDepth > 0)
    return 0;
  return identifierConstant(&parser.previous);
}

static void variable() {
  Token name = parser.previous;
  uint32_t hash = hashString(name.start, name.length);

  TrieNode *phrasalRoot = NULL;
  if (signatureTrieRoot != NULL) {
    for (int i = 0; i < signatureTrieRoot->childCount; i++) {
      if (signatureTrieRoot->children[i]->hash == hash) {
        phrasalRoot = signatureTrieRoot->children[i];
        break;
      }
    }
  }

  if (phrasalRoot != NULL) {
    int totalArgCount = 0;
    TrieNode *currentPhrase = phrasalRoot;

    for (;;) {
      activePhraseNode = currentPhrase;

      for (int i = 0; i < currentPhrase->segmentArity; i++) {
        expression();
        totalArgCount++;

        if (i < currentPhrase->segmentArity - 1) {
          consume(TOKEN_COMMA, "Expect ',' between arguments in segment.");
        }
      }

      activePhraseNode = NULL;

      if (currentPhrase->isTerminal) {
        bool matchedChild = false;
        if (isLabelToken(&parser.current)) {
          uint32_t nextHash =
              hashString(parser.current.start, parser.current.length);
          for (int c = 0; c < currentPhrase->childCount; c++) {
            if (currentPhrase->children[c]->hash == nextHash) {
              matchedChild = true;
              break;
            }
          }
        }
        if (!matchedChild)
          break;
      }

      if (!isLabelToken(&parser.current)) {
        error("Expect next label in phrasal function.");
        return;
      }
      advance();

      uint32_t nextHash =
          hashString(parser.previous.start, parser.previous.length);
      TrieNode *nextNode = NULL;
      for (int c = 0; c < currentPhrase->childCount; c++) {
        if (currentPhrase->children[c]->hash == nextHash) {
          nextNode = currentPhrase->children[c];
          break;
        }
      }

      if (nextNode == NULL) {
        error("Invalid phrasing for function call. Label doesn't match "
              "signature.");
        return;
      }
      currentPhrase = nextNode;
    }

    Token syntheticName;
    syntheticName.start = currentPhrase->stitchedName;
    syntheticName.length = (int)strlen(currentPhrase->stitchedName);
    uint8_t globalName = identifierConstant(&syntheticName);

    emitBytes(OP_GET_GLOBAL, globalName);
    emitBytes(OP_CALL_PHRASAL, (uint8_t)totalArgCount);
    return;
  }

  uint8_t getOp;
  int arg = resolveLocal(current, &name);

  if (arg != -1) {
    getOp = OP_GET_LOCAL;
  } else {
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
  }

  emitBytes(getOp, (uint8_t)arg);
}

// ==========================================
// LITERALS
// ==========================================

static void number() {
  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

static ObjString *parseStringFromToken(Token token) {
  return copyString(token.start + 1, token.length - 2);
}

static void string() {
  Token *t = &parser.previous;
  emitConstant(OBJ_VAL(copyStringUnescaped(t->start + 1, t->length - 2)));

  int partCount = 1;

  while (parser.previous.type == TOKEN_STRING_OPEN ||
         parser.previous.type == TOKEN_STRING_MIDDLE) {

    // BLINDFOLD
    TrieNode *enclosingPhrase = activePhraseNode;
    activePhraseNode = NULL;

    expression();

    // REMOVE BLINDFOLD
    activePhraseNode = enclosingPhrase;

    partCount++;

    if (partCount > 255)
      error("Too many parts in interpolated string.");

    advance();

    if (parser.previous.type == TOKEN_STRING_MIDDLE) {
      emitConstant(OBJ_VAL(parseStringFromToken(parser.previous)));
      partCount++;
    } else if (parser.previous.type == TOKEN_STRING_CLOSE) {
      emitConstant(OBJ_VAL(parseStringFromToken(parser.previous)));
      partCount++;
      break;
    } else {
      error("Expect end of string interpolation.");
    }
  }

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
    return;
  }
}

// ==========================================
// EXPRESSIONS
// ==========================================

static void grouping() {
  ignoreNewlines();

  // THE BLINDFOLD
  TrieNode *enclosingPhrase = activePhraseNode;
  activePhraseNode = NULL;

  expression();

  // REMOVE BLINDFOLD
  activePhraseNode = enclosingPhrase;

  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary() {
  TokenType operatorType = parser.previous.type;
  parsePrecedence(PREC_UNARY);

  switch (operatorType) {
  case TOKEN_MINUS:
    emitByte(OP_NEGATE);
    break;
  case TOKEN_NOT:
    emitByte(OP_NOT);
    break;
  default:
    return;
  }
}

static void binary() {
  TokenType operatorType = parser.previous.type;
  ParseRule *rule = getRule(operatorType);

  bool invert = false;
  if (operatorType == TOKEN_IS && match(TOKEN_NOT)) {
    invert = true;
  }

  ignoreNewlines();
  parsePrecedence((Precedence)(rule->precedence + 1));

  switch (operatorType) {
  case TOKEN_IS:
  case TOKEN_EQUAL_EQUAL:
  case TOKEN_EQUAL:
    emitByte(OP_EQUAL);
    if (invert)
      emitByte(OP_NOT);
    break;
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
    return;
  }
}

static void and_() {
  int endJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  ignoreNewlines();
  parsePrecedence(PREC_AND);
  patchJump(endJump);
}

static void or_() {
  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  int endJump = emitJump(OP_JUMP);
  patchJump(elseJump);
  emitByte(OP_POP);
  ignoreNewlines();
  parsePrecedence(PREC_OR);
  patchJump(endJump);
}

static uint8_t argumentList() {
  uint8_t argCount = 0;

  TrieNode *enclosingPhrase = activePhraseNode;
  activePhraseNode = NULL;

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      ignoreNewlines();
      expression();
      if (argCount >= 255)
        error("Can't have more than 255 arguments.");
      argCount++;
    } while (match(TOKEN_COMMA));
  }

  ignoreNewlines();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");

  activePhraseNode = enclosingPhrase;
  return argCount;
}

static void call() {
  uint8_t argCount = argumentList();
  emitBytes(OP_CALL, argCount);
}

static void list() {
  int itemCount = 0;
  ignoreNewlines();

  TrieNode *enclosingPhrase = activePhraseNode;
  activePhraseNode = NULL;

  if (!check(TOKEN_RIGHT_BRACKET)) {
    do {
      ignoreNewlines();
      if (check(TOKEN_RIGHT_BRACKET))
        break;
      expression();
      if (itemCount == 255)
        error("Can't have more than 255 items in a list.");
      itemCount++;
    } while (match(TOKEN_COMMA));
  }

  ignoreNewlines();
  consume(TOKEN_RIGHT_BRACKET, "Expect ']' after list.");
  emitBytes(OP_BUILD_LIST, (uint8_t)itemCount);

  activePhraseNode = enclosingPhrase;
}

static void subscript() {
  addLocal(syntheticToken(".target"));
  markInitialized(); // Note: markInitialized checks depth inside emitter.c

  TrieNode *enclosingPhrase = activePhraseNode;
  activePhraseNode = NULL;

  expression();

  activePhraseNode = enclosingPhrase;

  consume(TOKEN_RIGHT_BRACKET, "Expect ']' after index.");
  current->localCount--;
  emitByte(OP_GET_SUBSCRIPT);
}

static void dot() {
  parsePrecedence(PREC_CALL);
  emitByte(OP_GET_SUBSCRIPT);
}

static void possessive() {
  consume(TOKEN_IDENTIFIER, "Expect property name after 's.");
  uint8_t name = identifierConstant(&parser.previous);
  emitBytes(OP_CONSTANT, name);
  emitByte(OP_GET_SUBSCRIPT);
}

static void range() {
  parsePrecedence(PREC_TERM);

  if (match(TOKEN_BY)) {
    parsePrecedence(PREC_TERM);
  } else {
    emitConstant(NUMBER_VAL(1.0));
  }
  emitByte(OP_RANGE);
}

static void endKeyword() {
  Token targetName = syntheticToken(".target");
  int arg = resolveLocal(current, &targetName);

  if (arg == -1) {
    error("Can only use 'end' inside a list index or slice.");
    return;
  }

  emitBytes(OP_GET_LOCAL, (uint8_t)arg);
  emitByte(OP_GET_END_INDEX);
}

// ==========================================
// PRATT PARSER TABLE
// ==========================================

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
    [TOKEN_LEFT_BRACKET] = {list, subscript, PREC_CALL},
    [TOKEN_POSSESSIVE] = {NULL, possessive, PREC_CALL},
    [TOKEN_RIGHT_BRACKET] = {NULL, NULL, PREC_NONE},
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
  int exprStart = currentChunk()->count;

  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    error("Expect expression.");
    return;
  }

  prefixRule();

  while (precedence <= getRule(parser.current.type)->precedence) {
    if (activePhraseNode != NULL && isLabelToken(&parser.current)) {
      uint32_t currentHash =
          hashString(parser.current.start, parser.current.length);
      bool isNextLabel = false;
      for (int c = 0; c < activePhraseNode->childCount; c++) {
        if (activePhraseNode->children[c]->hash == currentHash) {
          isNextLabel = true;
          break;
        }
      }
      if (isNextLabel)
        break;
    }

    if (parser.current.type == TOKEN_IF) {
      if (!checkTernary())
        return; // Statement Guard fallback

      advance();

      CodeBuffer lhsBuffer;
      snipCode(&lhsBuffer, exprStart);
      expression();

      int jumpToElse = emitJump(OP_JUMP_IF_FALSE);
      emitByte(OP_POP);
      pasteCode(&lhsBuffer);
      int jumpToEnd = emitJump(OP_JUMP);

      patchJump(jumpToElse);
      emitByte(OP_POP);

      consume(TOKEN_ELSE, "Expect 'else' after ternary condition.");
      expression();
      patchJump(jumpToEnd);
      continue;
    }

    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule();
  }
}

static void expression() { parsePrecedence(PREC_ASSIGNMENT); }

// ==========================================
// STATEMENTS
// ==========================================

static void showBody() {
  expression();
  emitByte(OP_PRINT);
}

static void expressionBody() {
  expression();
  emitByte(OP_POP);
}

static void returnBody() {
  if (current->type == TYPE_SCRIPT) {
    error("Can't return from top-level code.");
  }

  if (check(TOKEN_NEWLINE) || check(TOKEN_EOF) || check(TOKEN_END) ||
      check(TOKEN_ELSE) || check(TOKEN_RIGHT_BRACE)) {
    emitReturn();
  } else {
    expression();
    emitByte(OP_RETURN);
  }
}

typedef enum { TARGET_LOCAL, TARGET_GLOBAL, TARGET_COMPLEX } TargetType;
typedef struct {
  TargetType type;
  Token rootName;
  int arg;
  CodeBuffer drillCode;
} SetTarget;

static void setBody() {
  SetTarget targets[255];
  int targetCount = 0;

  do {
    if (targetCount >= 255)
      error("Can't set more than 255 variables in one statement.");
    SetTarget *target = &targets[targetCount++];

    target->rootName = parser.current;
    consume(TOKEN_IDENTIFIER, "Expect variable name.");

    if (check(TOKEN_LEFT_BRACKET) || check(TOKEN_DOT) ||
        check(TOKEN_POSSESSIVE)) {
      target->type = TARGET_COMPLEX;
      int drillStart = currentChunk()->count;

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
          advance();
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
      int arg = resolveLocal(current, &target->rootName);
      if (arg != -1) {
        target->type = TARGET_LOCAL;
        target->arg = arg;
      } else {
        target->type = TARGET_GLOBAL;
        target->arg = identifierConstant(&target->rootName);
      }
      target->drillCode.count = 0;
    }
  } while (match(TOKEN_COMMA));

  consume(TOKEN_TO, "Expect 'to' after variable name(s).");

  int valCount = 0;
  do {
    if (current->localCount >= 255)
      error("Too many variables in scope to perform parallel assignment.");
    expression();
    addLocal(syntheticToken(".temp"));
    markInitialized();
    valCount++;
  } while (match(TOKEN_COMMA));

  if (valCount > 1 && valCount != targetCount) {
    error("Value count does not match variable count.");
  }

  int tempBaseSlot = current->localCount - valCount;

  for (int i = 0; i < targetCount; i++) {
    SetTarget *target = &targets[i];
    int tempSlot = tempBaseSlot + (valCount == 1 ? 0 : i);

    if (target->type == TARGET_COMPLEX) {
      int rootArg = resolveLocal(current, &target->rootName);
      if (rootArg != -1) {
        emitBytes(OP_GET_LOCAL, (uint8_t)rootArg);
      } else {
        emitBytes(OP_GET_GLOBAL, identifierConstant(&target->rootName));
      }

      pasteCode(&target->drillCode);
      emitBytes(OP_GET_LOCAL, (uint8_t)tempSlot);
      emitByte(OP_SET_SUBSCRIPT);
      emitByte(OP_POP);
    } else {
      emitBytes(OP_GET_LOCAL, (uint8_t)tempSlot);
      uint8_t setOp =
          (target->type == TARGET_LOCAL) ? OP_SET_LOCAL : OP_SET_GLOBAL;
      emitBytes(setOp, (uint8_t)target->arg);
      emitByte(OP_POP);
    }
  }

  for (int i = 0; i < valCount; i++) {
    emitByte(OP_POP);
    current->localCount--;
  }
}

static void statementWithGuard(StatementFn bodyFn) {
  ScopedChunk scope;
  beginScopeChunk(&scope);
  bodyFn();
  endScopeChunk(&scope);

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

static void block(TokenType *terminators, int count) {
  while (true) {
    ignoreNewlines();
    if (checkTerminator(terminators, count))
      return;
    if (check(TOKEN_EOF))
      return;
    declaration();
  }
}

static void ifStatement();

static void ifLogic(bool invert) {
  expression();
  if (invert)
    emitByte(OP_NOT);

  if (match(TOKEN_COLON)) {
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);

    TokenType thenEnds[] = {TOKEN_ELSE, TOKEN_END};
    block(thenEnds, 2);

    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE)) {
      if (match(TOKEN_IF)) {
        ifStatement();
      } else if (match(TOKEN_COLON)) {
        TokenType elseEnds[] = {TOKEN_END};
        block(elseEnds, 1);
        consume(TOKEN_END, "Expect 'end' after else block.");
      } else {
        statement();
      }
    } else {
      consume(TOKEN_END, "Expect 'end' after if block.");
    }
    patchJump(elseJump);
  } else {
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE)) {
      statement();
    }
    patchJump(elseJump);
  }
}

static void ifStatement() { ifLogic(false); }
static void unlessStatement() { ifLogic(true); }

static void loopLogic(bool invert) {
  int loopStart = currentChunk()->count;
  expression();
  if (invert)
    emitByte(OP_NOT);

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);

  if (match(TOKEN_COLON)) {
    TokenType terminators[] = {TOKEN_END};
    block(terminators, 1);
    consume(TOKEN_END, "Expect 'end' after loop.");
  } else {
    statement();
  }

  emitLoop(loopStart);
  patchJump(exitJump);
  emitByte(OP_POP);
}

static void whileStatement() { loopLogic(false); }
static void untilStatement() { loopLogic(true); }

static void forStatement() {
  beginScope();

  if (check(TOKEN_EACH))
    advance();

  Token iteratorName = parser.current;
  consume(TOKEN_IDENTIFIER, "Expect variable name.");
  consume(TOKEN_IN, "Expect 'in' after variable name.");

  expression();
  emitByte(OP_GET_ITER);

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

  if (match(TOKEN_COLON)) {
    TokenType terminators[] = {TOKEN_END};
    block(terminators, 1);
    consume(TOKEN_END, "Expect 'end' after loop.");
  } else {
    statement();
  }

  emitByte(OP_POP);
  emitLoop(loopStart);
  patchJump(exitJump);

  current->localCount--;
  endScope();
}

static void statement() {
  ignoreNewlines();

  if (match(TOKEN_SHOW)) {
    statementWithGuard(showBody);
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_UNLESS)) {
    unlessStatement();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
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
    consumeEnd();
    endScope();
  } else {
    statementWithGuard(expressionBody);
  }
}

// ==========================================
// DECLARATIONS
// ==========================================

static TrieNode *function(FunctionType type, Token *rootToken) {
  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope();

  TrieNode *currentNode = signatureTrieRoot;
  uint32_t firstHash = hashString(rootToken->start, rootToken->length);
  bool foundRoot = false;

  if (currentNode != NULL) {
    for (int i = 0; i < currentNode->childCount; i++) {
      if (currentNode->children[i]->hash == firstHash) {
        currentNode = currentNode->children[i];
        foundRoot = true;
        break;
      }
    }
  }

  if (!foundRoot)
    currentNode = NULL;

  for (;;) {
    if (match(TOKEN_LEFT_PAREN)) {
      if (!check(TOKEN_RIGHT_PAREN)) {
        do {
          current->function->arity++;
          if (current->function->arity > 255) {
            errorAtCurrent("Can't have more than 255 parameters.");
          }
          uint8_t constant = parseVariable("Expect parameter name.");
          defineVariable(constant);
        } while (match(TOKEN_COMMA));
      }
      consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    }

    if (currentNode != NULL && !currentNode->isTerminal &&
        isLabelToken(&parser.current)) {
      advance();
      uint32_t nextHash =
          hashString(parser.previous.start, parser.previous.length);
      TrieNode *nextNode = NULL;
      for (int i = 0; i < currentNode->childCount; i++) {
        if (currentNode->children[i]->hash == nextHash) {
          nextNode = currentNode->children[i];
          break;
        }
      }
      currentNode = nextNode;
    } else {
      break;
    }
  }

  consume(TOKEN_COLON, "Expect ':' before function body.");

  while (!check(TOKEN_END) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_END, "Expect 'end' after function body.");

  ObjFunction *function = endCompiler();

  if (currentNode != NULL && currentNode->isTerminal &&
      currentNode->stitchedName != NULL) {
    function->name = copyString(currentNode->stitchedName,
                                strlen(currentNode->stitchedName));
  } else {
    function->name = copyString(rootToken->start, rootToken->length);
  }

  emitBytes(OP_CONSTANT, makeConstant(OBJ_VAL(function)));
  return currentNode;
}

static void varDeclaration() {
  bool isFunction = false;
  Token token3 = tokenStream[currentTokenIndex];

  if (token3.type == TOKEN_LEFT_PAREN || token3.type == TOKEN_COLON ||
      isLabelToken(&token3)) {
    isFunction = true;
  }

  if (isFunction) {
    consume(TOKEN_IDENTIFIER, "Expect function name.");
    Token rootToken = parser.previous;

    if (current->scopeDepth > 0) {
      addLocal(rootToken);
      markInitialized();
    }

    TrieNode *terminalNode = function(TYPE_FUNCTION, &rootToken);

    if (current->scopeDepth == 0) {
      uint8_t globalName;
      if (terminalNode != NULL && terminalNode->stitchedName != NULL) {
        Token syntheticName;
        syntheticName.start = terminalNode->stitchedName;
        syntheticName.length = strlen(terminalNode->stitchedName);
        globalName = identifierConstant(&syntheticName);
      } else {
        globalName = identifierConstant(&rootToken);
      }

      emitBytes(OP_DEFINE_GLOBAL, globalName);
      emitByte(OP_POP);
    }
    return;
  }

  uint8_t globals[255];
  int varCount = 0;

  do {
    if (varCount >= 255)
      error("Can't declare more than 255 variables in one statement.");

    if (current->scopeDepth > 0) {
      consume(TOKEN_IDENTIFIER, "Expect variable name.");
      Token *name = &parser.previous;

      for (int i = current->localCount - 1; i >= 0; i--) {
        Local *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth)
          break;
        if (identifiersEqual(name, &local->name)) {
          error("Already a variable with this name in this scope.");
        }
      }
      addLocal(*name);
      globals[varCount] = 0;
    } else {
      globals[varCount] = parseVariableConstant("Expect variable name.");
    }
    varCount++;
  } while (match(TOKEN_COMMA));

  if (match(TOKEN_BE)) {
    ignoreNewlines();

    int valCount = 0;
    do {
      expression();
      valCount++;
    } while (match(TOKEN_COMMA));

    if (valCount > 1 && valCount != varCount) {
      error("Value count does not match variable count.");
    }
    consumeEnd();

    if (current->scopeDepth > 0) {
      if (valCount == 1 && varCount > 1) {
        int firstLocalSlot = current->localCount - varCount;
        for (int i = 1; i < varCount; i++) {
          emitBytes(OP_GET_LOCAL, firstLocalSlot);
        }
      }
      for (int i = 0; i < varCount; i++) {
        current->locals[current->localCount - 1 - i].depth =
            current->scopeDepth;
      }
    } else {
      if (valCount == 1) {
        for (int i = varCount - 1; i >= 0; i--) {
          emitBytes(OP_DEFINE_GLOBAL, globals[i]);
        }
        emitByte(OP_POP);
      } else {
        for (int i = varCount - 1; i >= 0; i--) {
          emitBytes(OP_DEFINE_GLOBAL, globals[i]);
          emitByte(OP_POP);
        }
      }
    }
  } else {
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

static void declaration() {
  ignoreNewlines();
  if (check(TOKEN_EOF))
    return;

  if (match(TOKEN_LET)) {
    varDeclaration();
  } else {
    statement();
  }

  if (parser.panicMode)
    synchronize();
}

// ==========================================
// MAIN COMPILE ENTRY
// ==========================================

ObjFunction *compile(const char *source) {
  // 1. Initialize our pre-processor (Lexes & caches all tokens)
  initScout(source);

  // 2. Map out the English grammar Trie
  hoistSignatures();

  // 3. Initialize Main Compiler
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);
  current = &compiler;

  parser.hadError = false;
  parser.panicMode = false;

  advance(); // Pull first token from cache

  // 4. The Main Pass
  while (!match(TOKEN_EOF)) {
    declaration();
  }

  emitReturn();
  ObjFunction *function = compiler.function;

  // 5. Cleanup
  freeScout();

  if (parser.hadError)
    return NULL;
  return function;
}
