#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "cJSON.h"
#include "error.h"
#include "lsp.h"
#include "parser.h"
#include "vm.h"
#include <stdarg.h>

// ==========================================
// THE TRUE BRAIN: SEMANTIC SYMBOL TABLE
// ==========================================

// 1. What kind of data is this variable holding?
typedef enum {
  LSP_TYPE_UNKNOWN,
  LSP_TYPE_NUMBER,
  LSP_TYPE_STRING,
  LSP_TYPE_BOOLEAN,
  LSP_TYPE_LIST,
  LSP_TYPE_DICTIONARY,
  LSP_TYPE_FUNCTION,
  LSP_TYPE_BLUEPRINT,
  LSP_TYPE_UNION,
} LspSymbolType;

// --- THE SPELLCHECKING ORACLE (For the LSP) ---
static int levenshteinDistanceLsp(const char *s1, const char *s2) {
  int len1 = strlen(s1);
  int len2 = strlen(s2);

  // Use a fast stack array (Max length 63 + 1)
  int column[64];

  for (int i = 0; i <= len1; i++)
    column[i] = i;

  for (int x = 1; x <= len2; x++) {
    int lastDiagonal = column[0];
    column[0] = x;
    for (int y = 1; y <= len1; y++) {
      int oldDiagonal = column[y];
      int cost = (s1[y - 1] == s2[x - 1]) ? 0 : 1;

      int min = column[y] + 1; // Deletion
      if (column[y - 1] + 1 < min)
        min = column[y - 1] + 1; // Insertion
      if (lastDiagonal + cost < min)
        min = lastDiagonal + cost; // Substitution

      column[y] = min;
      lastDiagonal = oldDiagonal;
    }
  }
  return column[len1]; // No free() needed!
}

// --- THE SECRET SIDE-CHANNEL LOGGER ---
static void lspLog(const char *format, ...) {
  FILE *logFile = fopen("moon_lsp.log", "a");
  if (logFile) {
    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);
    fprintf(logFile, "\n");
    fclose(logFile);
  }
}

// 2. The Identity Card for every variable
typedef struct {
  char name[64];
  int depth;
  LspSymbolType type;
  char blueprintName[64]; // NEW: If it's a Blueprint, WHICH one is it? (e.g.,
                          // "Player")
  int line;
  int column;
} LspSymbol;

// 3. The Memory Palace (Storage)
static LspSymbol lspSymbols[1500];
static int lspSymbolCount = 0;

// --- THE BLUEPRINT REGISTRY ---
typedef struct {
  char name[64];           // e.g., "Player"
  char properties[20][64]; // e.g., ["health", "speed", "name"]
  int propCount;
} LspBlueprint;

static LspBlueprint lspBlueprints[50]; // Remember up to 50 custom types
static int lspBlueprintCount = 0;

static int currentScopeDepth = 0;

// --- PHASE 3 GLOBALS ---
static char *currentDocumentText = NULL;
static int targetCursorLine = -1;
static bool isWalkingPaused = false;

// ==========================================
// PHASE 4 GLOBALS (Semantic Tokens)
// ==========================================
typedef struct {
  int line;
  int character;
  int length;
  int type;      // The Legend Index
  int modifiers; // Bitmask (0 for now)
  int priority;  // 0 = Lexical, 1 = AST (Higher wins)
} SemanticToken;

static SemanticToken semTokens[10000];
static int semTokenCount = 0;

static void addSemToken(int line, int col, int len, int type, int priority) {
  if (semTokenCount >= 10000 || line <= 0)
    return; // Safety cap

  semTokens[semTokenCount].line = line - 1; // 0-Indexed
  semTokens[semTokenCount].character = col > 0 ? col - 1 : 0;
  semTokens[semTokenCount].length = len;
  semTokens[semTokenCount].type = type;
  semTokens[semTokenCount].modifiers = 0;
  semTokens[semTokenCount].priority = priority;
  semTokenCount++;
}

static void addMultilineSemToken(Token t, int type, int priority) {
  // 1. Calculate true start line by subtracting newlines
  int newlineCount = 0;
  for (int i = 0; i < t.length; i++) {
    if (t.start[i] == '\n') newlineCount++;
  }
  int currentLine = t.line - newlineCount;

  // 2. Calculate true start column by walking backwards to previous \n
  int currentCol = 1;
  extern char *currentDocumentText;
  if (currentDocumentText != NULL) {
    const char *p = t.start - 1;
    while (p >= currentDocumentText && *p != '\n') {
      currentCol++;
      p--;
    }
  }

  // 3. Emit tokens line by line
  int currentLen = 0;
  for (int i = 0; i < t.length; i++) {
    if (t.start[i] == '\n') {
      if (currentLen > 0) {
        addSemToken(currentLine, currentCol, currentLen, type, priority);
      }
      currentLine++;
      currentCol = 1; // 1-indexed column for the new line
      currentLen = 0;
    } else {
      currentLen++;
    }
  }
  if (currentLen > 0) {
    addSemToken(currentLine, currentCol, currentLen, type, priority);
  }
}

// QSort Comparator to organize tokens top-to-bottom, left-to-right
static int compareTokens(const void *a, const void *b) {
  SemanticToken *t1 = (SemanticToken *)a;
  SemanticToken *t2 = (SemanticToken *)b;
  if (t1->line != t2->line)
    return t1->line - t2->line;
  if (t1->character != t2->character)
    return t1->character - t2->character;
  
  // If line and character match, sort by priority (Ascending, so highest priority is LAST)
  return t1->priority - t2->priority;
}

// --- THE DOCSTRING EXTRACTOR ---
static void extractDocstring(int targetLine, char *output, int maxLen) {
  output[0] = '\0';
  if (targetLine <= 1 || currentDocumentText == NULL)
    return;

  int currentL = 1;
  char *ptr = currentDocumentText;
  char *prevLineStart = NULL;

  // Find the start of the line immediately above the target
  while (*ptr != '\0' && currentL < targetLine) {
    if (currentL == targetLine - 1) {
      if (prevLineStart == NULL)
        prevLineStart = ptr;
    }
    if (*ptr == '\n')
      currentL++;
    ptr++;
  }

  if (prevLineStart != NULL) {
    while (*prevLineStart == ' ' || *prevLineStart == '\t')
      prevLineStart++;
    if (*prevLineStart == '#') {
      prevLineStart++;
      while (*prevLineStart == ' ' || *prevLineStart == '#')
        prevLineStart++; // Support ## docs!

      char *end = prevLineStart;
      while (*end != '\n' && *end != '\0')
        end++;

      int len = end - prevLineStart;
      if (len > maxLen - 1)
        len = maxLen - 1;
      strncpy(output, prevLineStart, len);
      output[len] = '\0';
    }
  }
}

// --- THE MEMORY MANAGERS ---

// 1. Adds a variable to the current room
static void registerSymbol(const char *name, LspSymbolType type,
                           const char *blueprintName, int line,
                           int column) { // <--- ADD PARAMS
  if (lspSymbolCount >= 1500)
    return;

  for (int i = 0; i < lspSymbolCount; i++) {
    if (strcmp(lspSymbols[i].name, name) == 0 &&
        lspSymbols[i].depth == currentScopeDepth) {
      return;
    }
  }

  strncpy(lspSymbols[lspSymbolCount].name, name, 63);
  lspSymbols[lspSymbolCount].name[63] = '\0';
  lspSymbols[lspSymbolCount].depth = currentScopeDepth;
  lspSymbols[lspSymbolCount].type = type;

  if (blueprintName != NULL) {
    strncpy(lspSymbols[lspSymbolCount].blueprintName, blueprintName, 63);
    lspSymbols[lspSymbolCount].blueprintName[63] = '\0';
  } else {
    lspSymbols[lspSymbolCount].blueprintName[0] = '\0';
  }

  // --- NEW: Store the coordinates! ---
  lspSymbols[lspSymbolCount].line = line;
  lspSymbols[lspSymbolCount].column = column;

  lspLog("[BRAIN] Registered '%s' at depth %d (Line %d)", name,
         currentScopeDepth, line);
  lspSymbolCount++;
}

// 1.5. Adds a Blueprint definition to the Registry
static void registerBlueprint(const char *name, Token *props, int propCount) {
  if (lspBlueprintCount >= 50)
    return;

  strncpy(lspBlueprints[lspBlueprintCount].name, name, 63);
  lspBlueprints[lspBlueprintCount].name[63] = '\0';

  int maxProps = propCount < 20 ? propCount : 20;
  lspBlueprints[lspBlueprintCount].propCount = maxProps;

  for (int i = 0; i < maxProps; i++) {
    int pLen = props[i].length < 63 ? props[i].length : 63;
    strncpy(lspBlueprints[lspBlueprintCount].properties[i], props[i].start,
            pLen);
    lspBlueprints[lspBlueprintCount].properties[i][pLen] = '\0';
  }

  lspLog("[REGISTRY] Blueprint '%s' memorized with %d properties.", name,
         maxProps);
  lspBlueprintCount++;
}

// 2. Destroys variables when we hit an 'end' keyword
static void popScope() {
  if (isWalkingPaused)
    return; // THE FREEZE FRAME

  // PROTECT THE GLOBAL SCOPE! (Depth 1 in MOON)
  // We never destroy global variables so they are always available.
  if (currentScopeDepth <= 1) {
    currentScopeDepth--;
    return;
  }

  lspLog("[BRAIN] Leaving depth %d...", currentScopeDepth);
  while (lspSymbolCount > 0 &&
         lspSymbols[lspSymbolCount - 1].depth == currentScopeDepth) {
    lspSymbolCount--;
  }
  currentScopeDepth--;
}

// --- THE SILENT OBSERVER (AST WALKER) ---
static void analyzeNode(Node *node) {
  if (node == NULL || isWalkingPaused)
    return;

  // THE FREEZE FRAME! If we reach the cursor's exact line, stop walking
  // instantly!
  if (targetCursorLine != -1 && node->line >= targetCursorLine) {
    isWalkingPaused = true;
    return;
  }

  switch (node->type) {
  // --- THE ROOMS (Scope Management) ---
  case NODE_BLOCK: {
    currentScopeDepth++;
    for (int i = 0; i < node->as.block.count; i++) {
      if (isWalkingPaused)
        break; // Abort if a child froze us!
      analyzeNode(node->as.block.statements[i]);
    }
    // Only destroy the room if the cursor IS NOT inside it!
    if (!isWalkingPaused) {
      popScope();
    }
    break;
  }

  case NODE_FUNCTION: {
    // 1. Register the function name in the CURRENT scope
    char funcName[64];
    int len =
        node->as.function.name.length < 63 ? node->as.function.name.length : 63;
    strncpy(funcName, node->as.function.name.start, len);
    funcName[len] = '\0';
    registerSymbol(funcName, LSP_TYPE_FUNCTION, NULL,
                   node->as.function.name.line, node->as.function.name.column);

    // THE FIX: Lex the function signature from the source code instead of using the mangled length!
    if (currentDocumentText != NULL && node->as.function.name.line > 0) {
      int targetLine = node->as.function.name.line;
      int currentCol = node->as.function.name.column;
      
      const char *ptr = currentDocumentText;
      int currLine = 1;
      while (*ptr != '\0' && currLine < targetLine) {
        if (*ptr == '\n') currLine++;
        ptr++;
      }
      
      int c = 1;
      while (*ptr != '\0' && c < currentCol) {
        ptr++;
        c++;
      }
      
      while (*ptr != '\0' && *ptr != '(' && *ptr != ':' && *ptr != '\n') {
        if ((*ptr >= 'a' && *ptr <= 'z') || (*ptr >= 'A' && *ptr <= 'Z') || *ptr == '_') {
          int wLen = 0;
          while ((ptr[wLen] >= 'a' && ptr[wLen] <= 'z') || 
                 (ptr[wLen] >= 'A' && ptr[wLen] <= 'Z') || 
                 (ptr[wLen] >= '0' && ptr[wLen] <= '9') || 
                 ptr[wLen] == '_') {
            wLen++;
          }
          addSemToken(targetLine, c, wLen, 12, 1);
          ptr += wLen;
          c += wLen;
        } else {
          ptr++;
          c++;
        }
      }
    }

    // 2. Go one level deeper for parameters and body!
    currentScopeDepth++;
    for (int i = 0; i < node->as.function.paramCount; i++) {
      char paramName[64];
      int pLen = node->as.function.parameters[i].length < 63
                     ? node->as.function.parameters[i].length
                     : 63;
      strncpy(paramName, node->as.function.parameters[i].start, pLen);
      paramName[pLen] = '\0';
      registerSymbol(paramName, LSP_TYPE_UNKNOWN, NULL,
                     node->as.function.parameters[i].line,
                     node->as.function.parameters[i].column);
    }

    analyzeNode(node->as.function.body);

    // Do not pop if the cursor is inside the function!
    if (!isWalkingPaused)
      popScope();
    break;
  }

  case NODE_FOR: {
    currentScopeDepth++; // The iterator variable needs its own scope
    char iterName[64];
    int len = node->as.forStmt.iterator.length < 63
                  ? node->as.forStmt.iterator.length
                  : 63;
    strncpy(iterName, node->as.forStmt.iterator.start, len);
    iterName[len] = '\0';
    registerSymbol(iterName, LSP_TYPE_UNKNOWN, NULL,
                   node->as.forStmt.iterator.line,
                   node->as.forStmt.iterator.column);

    if (node->as.forStmt.hasIndex) {
      char idxName[64];
      int idxLen = node->as.forStmt.indexVar.length < 63
                       ? node->as.forStmt.indexVar.length
                       : 63;
      strncpy(idxName, node->as.forStmt.indexVar.start, idxLen);
      idxName[idxLen] = '\0';
      registerSymbol(idxName, LSP_TYPE_NUMBER, NULL,
                     node->as.forStmt.indexVar.line,
                     node->as.forStmt.indexVar.column);
    }
    // ---------------------------------------------

    analyzeNode(node->as.forStmt.sequence);
    analyzeNode(node->as.forStmt.body);

    // Do not pop if the cursor is inside the loop!
    if (!isWalkingPaused)
      popScope();
    break;
  }

  case NODE_COMPREHENSION: {
    currentScopeDepth++;
    char iterName[64];
    int len = node->as.comprehension.iterator.length < 63
                  ? node->as.comprehension.iterator.length
                  : 63;
    strncpy(iterName, node->as.comprehension.iterator.start, len);
    iterName[len] = '\0';
    registerSymbol(iterName, LSP_TYPE_UNKNOWN, NULL,
                   node->as.comprehension.iterator.line,
                   node->as.comprehension.iterator.column);

    if (node->as.comprehension.hasIndex) {
      char idxName[64];
      int idxLen = node->as.comprehension.indexVar.length < 63
                       ? node->as.comprehension.indexVar.length
                       : 63;
      strncpy(idxName, node->as.comprehension.indexVar.start, idxLen);
      idxName[idxLen] = '\0';
      registerSymbol(idxName, LSP_TYPE_NUMBER, NULL,
                   node->as.comprehension.indexVar.line,
                   node->as.comprehension.indexVar.column);
    }

    analyzeNode(node->as.comprehension.sequence);
    analyzeNode(node->as.comprehension.body);

    if (!isWalkingPaused)
      popScope();
    break;
  }

  case NODE_KEEP:
    analyzeNode(node->as.keepStmt.key);
    analyzeNode(node->as.keepStmt.value);
    break;

    // --- THE VARIABLES (Memory Registration) ---
  case NODE_LET: {
    for (int i = 0; i < node->as.let.nameCount; i++) {
      LspSymbolType guessedType = LSP_TYPE_UNKNOWN;
      char *bpName = NULL; // Keep track of the blueprint!

      Node *expr = (node->as.let.exprCount == 1) ? node->as.let.exprs[0]
                                                 : node->as.let.exprs[i];
      if (expr != NULL) {
        if (expr->type == NODE_LITERAL) {
          if (IS_NUMBER(expr->as.literal.value))
            guessedType = LSP_TYPE_NUMBER;
          else if (IS_STRING(expr->as.literal.value))
            guessedType = LSP_TYPE_STRING;
          else if (IS_BOOL(expr->as.literal.value))
            guessedType = LSP_TYPE_BOOLEAN;
        } else if (expr->type == NODE_LIST) {
          guessedType = LSP_TYPE_LIST;
        } else if (expr->type == NODE_DICT) {
          guessedType = LSP_TYPE_DICTIONARY;
        } else if (expr->type == NODE_INSTANTIATE) {
          guessedType = LSP_TYPE_BLUEPRINT;

          // THE MISSING LINK: Grab the target Blueprint name!
          // (Assuming target is a VARIABLE node, e.g., 'Player')
          if (expr->as.instantiate.target->type == NODE_VARIABLE) {
            static char tempName[64];
            Token t = expr->as.instantiate.target->as.variable.name;
            int len = t.length < 63 ? t.length : 63;
            strncpy(tempName, t.start, len);
            tempName[len] = '\0';
            bpName = tempName;
          }
        } else if (expr->type == NODE_UNION_TYPE) {
          guessedType = LSP_TYPE_UNION;
        }

        analyzeNode(expr);
      }

      char varName[64];
      int len =
          node->as.let.names[i].length < 63 ? node->as.let.names[i].length : 63;
      strncpy(varName, node->as.let.names[i].start, len);
      varName[len] = '\0';

      registerSymbol(varName, guessedType, bpName, node->as.let.names[i].line,
                     node->as.let.names[i].column);

      // Color the variable declaration! (Type 8 = Variable)
      // Ignore ghost tokens with column == 0
      if (node->as.let.names[i].column > 0) {
        addSemToken(node->as.let.names[i].line, node->as.let.names[i].column,
                    node->as.let.names[i].length, 8, 1);
      }
    }
    break;
  }

    // --- THE STRICT SENTINEL (Semantic Errors) ---
  case NODE_VARIABLE: {
    char varName[64];
    Token t = node->as.variable.name;
    int len = t.length < 63 ? t.length : 63;
    strncpy(varName, t.start, len);
    varName[len] = '\0';

    bool found = false;

    // 1. Check local Memory Palace (Variables, Functions, Blueprints)
    for (int i = 0; i < lspSymbolCount; i++) {
      if (strcmp(lspSymbols[i].name, varName) == 0) {
        found = true;
        break;
      }
    }

    // 2. Check Native Core Types (Since the VM handles these globally)
    if (!found) {
      const char *natives[] = {"Number", "String",   "List", "Dict", "Bool",
                               "Range",  "Function", "Nil",  "Any",  "Type"};
      for (int i = 0; i < 10; i++) {
        if (strcmp(varName, natives[i]) == 0) {
          found = true;
          break;
        }
      }
    }

    // 3. Ignore internal primitives (e.g., __io_read)
    if (!found && len >= 2 && varName[0] == '_' && varName[1] == '_') {
      found = true;
    }

    // 4. Fire the Squiggle!
    if (!found && lspDiagnosticCount < 100) {
      Diagnostic *d = &lspDiagnostics[lspDiagnosticCount++];
      d->line = t.line;
      d->column = t.column;
      d->length = t.length > 0 ? t.length : 1;

      // The Oracle: Find a close match!
      const char *bestMatch = NULL;
      int minDistance = 999;
      int maxAllowed = (len <= 3) ? 1 : 2;

      // Check user-defined variables
      for (int i = 0; i < lspSymbolCount; i++) {
        int dist = levenshteinDistanceLsp(varName, lspSymbols[i].name);
        if (dist < minDistance && dist <= maxAllowed) {
          minDistance = dist;
          bestMatch = lspSymbols[i].name;
        }
      }

      // Check Native Types!
      const char *natives[] = {"Number", "String",   "List", "Dict", "Bool",
                               "Range",  "Function", "Nil",  "Any",  "Type"};
      for (int i = 0; i < 10; i++) {
        int dist = levenshteinDistanceLsp(varName, natives[i]);
        if (dist < minDistance && dist <= maxAllowed) {
          minDistance = dist;
          bestMatch = natives[i];
        }
      }

      if (bestMatch != NULL) {
        snprintf(d->message, sizeof(d->message),
                 "Reference Error: Undefined variable '%s'.\n\nHint: Did you "
                 "mean '%s'?",
                 varName, bestMatch);
      } else {
        snprintf(d->message, sizeof(d->message),
                 "Reference Error: Undefined variable '%s'.\n\nHint: Did you "
                 "misspell it or forget to declare it with 'let'?",
                 varName);
      }
    }

    if (found) {
      int tokenType = 8; // Default to Variable
      // Check the local memory palace to see if it's actually a Blueprint or
      // Function
      for (int i = 0; i < lspSymbolCount; i++) {
        if (strcmp(lspSymbols[i].name, varName) == 0) {
          if (lspSymbols[i].type == LSP_TYPE_BLUEPRINT)
            tokenType = 1;
          if (lspSymbols[i].type == LSP_TYPE_FUNCTION)
            tokenType = 12;
          break;
        }
      }
      // Native Type checks
      const char *natives[] = {"Number", "String",   "List", "Dict", "Bool",
                               "Range",  "Function", "Nil",  "Any",  "Type"};
      for (int i = 0; i < 10; i++) {
        if (strcmp(varName, natives[i]) == 0)
          tokenType = 1; // Native Types get Blueprint coloring
      }
      // Ghost tokens have column == 0, Moon is 1-indexed. We shouldn't highlight them!
      if (t.column > 0) {
        addSemToken(t.line, t.column, len, tokenType, 1);
      }
    }

    break;
  }

  case NODE_TYPE_DECL: {
    char typeName[64];
    int len =
        node->as.typeDecl.name.length < 63 ? node->as.typeDecl.name.length : 63;
    strncpy(typeName, node->as.typeDecl.name.start, len);
    typeName[len] = '\0';
    registerSymbol(typeName, LSP_TYPE_BLUEPRINT, NULL,
                   node->as.typeDecl.name.line, node->as.typeDecl.name.column);

    addSemToken(node->as.typeDecl.name.line, node->as.typeDecl.name.column, len,
                1, 1);

    registerBlueprint(typeName, node->as.typeDecl.propertyNames,
                      node->as.typeDecl.count);
    for (int i = 0; i < node->as.typeDecl.count; i++) {
      addSemToken(node->as.typeDecl.propertyNames[i].line,
                  node->as.typeDecl.propertyNames[i].column,
                  node->as.typeDecl.propertyNames[i].length, 9, 1);
      analyzeNode(node->as.typeDecl.defaultValues[i]);
    }
    break;
  }

  // --- THE RECURSION (Keep Walking!) ---
  case NODE_IF:
    analyzeNode(node->as.ifStmt.condition);
    analyzeNode(node->as.ifStmt.thenBranch);
    if (node->as.ifStmt.elseBranch)
      analyzeNode(node->as.ifStmt.elseBranch);
    break;

  case NODE_WHILE:
    analyzeNode(node->as.whileStmt.condition);
    analyzeNode(node->as.whileStmt.body);
    break;

  case NODE_SET:
    for (int i = 0; i < node->as.set.targetCount; i++)
      analyzeNode(node->as.set.targets[i]);
    for (int i = 0; i < node->as.set.valueCount; i++)
      analyzeNode(node->as.set.values[i]);
    break;

  case NODE_BINARY:
  case NODE_LOGICAL:
    analyzeNode(node->as.binary.left);
    analyzeNode(node->as.binary.right);
    break;

  case NODE_UNARY:
    analyzeNode(node->as.unary.right);
    break;



  case NODE_PHRASAL_CALL: {
    // 1. Color all structural words as Functions (12) and register them as aliases!
    char mangledBuffer[128];
    int mLen = node->as.phrasalCall.mangledName.length < 127 ? node->as.phrasalCall.mangledName.length : 127;
    strncpy(mangledBuffer, node->as.phrasalCall.mangledName.start, mLen);
    mangledBuffer[mLen] = '\0';

    for (int i = 0; i < node->as.phrasalCall.phraseTokenCount; i++) {
      Token pt = node->as.phrasalCall.phraseTokens[i];
      addSemToken(pt.line, pt.column, pt.length, 12, 1);

      // Register alias so Hover and Go-To Definition works on individual words!
      char aliasName[64];
      int pLen = pt.length < 63 ? pt.length : 63;
      strncpy(aliasName, pt.start, pLen);
      aliasName[pLen] = '\0';
      registerSymbol(aliasName, LSP_TYPE_FUNCTION, mangledBuffer, pt.line, pt.column);
    }

    // 2. Walk the arguments
    for (int i = 0; i < node->as.phrasalCall.argCount; i++)
      analyzeNode(node->as.phrasalCall.arguments[i]);
    break;
  }

  case NODE_EXPRESSION_STMT:
  case NODE_RETURN:
    analyzeNode(node->as.singleExpr.expression);
    break;

  case NODE_LIST:
    for (int i = 0; i < node->as.list.count; i++)
      analyzeNode(node->as.list.items[i]);
    break;

  case NODE_DICT:
    for (int i = 0; i < node->as.dictExpr.count; i++) {
      analyzeNode(node->as.dictExpr.keys[i]);
      analyzeNode(node->as.dictExpr.values[i]);
    }
    break;

  case NODE_INTERPOLATION:
    for (int i = 0; i < node->as.interpolation.partCount; i++)
      analyzeNode(node->as.interpolation.parts[i]);
    break;

  case NODE_PROPERTY:
    addSemToken(node->as.property.name.line, node->as.property.name.column,
                node->as.property.name.length, 9, 1);
    analyzeNode(node->as.property.target);
    break;

  case NODE_SUBSCRIPT:
    analyzeNode(node->as.subscript.left);
    analyzeNode(node->as.subscript.index);
    break;

  case NODE_RANGE:
    analyzeNode(node->as.range.start);
    analyzeNode(node->as.range.end);
    analyzeNode(node->as.range.step);
    break;

  case NODE_INSTANTIATE:
    analyzeNode(node->as.instantiate.target);
    for (int i = 0; i < node->as.instantiate.count; i++) {

      // --- THE KEY COLORING FIX ---
      // Send token type 9 (Property) to NeoVim!
      addSemToken(node->as.instantiate.propertyNames[i].line,
                  node->as.instantiate.propertyNames[i].column,
                  node->as.instantiate.propertyNames[i].length, 9, 1);

      analyzeNode(node->as.instantiate.values[i]);
    }
    break;

  case NODE_CAST:
    analyzeNode(node->as.cast.left);
    analyzeNode(node->as.cast.right);
    break;

  default:
    break;
  }
}

// --- THE JSON PRINTER ---
static void sendResponse(cJSON *response) {
  // 1. Turn the C-Struct into a raw JSON string
  char *jsonString = cJSON_PrintUnformatted(response);
  int length = (int)strlen(jsonString);

  // 2. LSP STRICT FORMATTING: Header \r\n\r\n Payload
  fprintf(stdout, "Content-Length: %d\r\n\r\n%s", length, jsonString);

  // 3. CRITICAL: You must flush stdout, or NeoVim will hang forever waiting!
  fflush(stdout);

  free(jsonString);
}

// --- THE SQUIGGLE GENERATOR ---
static void publishDiagnostics(const char *uri) {
  cJSON *notification = cJSON_CreateObject();
  cJSON_AddStringToObject(notification, "jsonrpc", "2.0");
  cJSON_AddStringToObject(notification, "method",
                          "textDocument/publishDiagnostics");

  cJSON *params = cJSON_CreateObject();
  cJSON_AddStringToObject(params, "uri", uri);

  cJSON *diagArray = cJSON_CreateArray();

  for (int i = 0; i < lspDiagnosticCount; i++) {
    Diagnostic *d = &lspDiagnostics[i];
    cJSON *diagObj = cJSON_CreateObject();

    cJSON_AddStringToObject(diagObj, "message", d->message);
    cJSON_AddNumberToObject(diagObj, "severity", 1); // 1 = Error (Red Squiggle)

    // LSP Coordinates are 0-Indexed! We must subtract 1 from MOON's 1-indexed
    // lines.
    cJSON *range = cJSON_CreateObject();
    cJSON *start = cJSON_CreateObject();
    cJSON_AddNumberToObject(start, "line", d->line > 0 ? d->line - 1 : 0);
    cJSON_AddNumberToObject(start, "character",
                            d->column > 0 ? d->column - 1 : 0);

    cJSON *end = cJSON_CreateObject();
    cJSON_AddNumberToObject(end, "line", d->line > 0 ? d->line - 1 : 0);
    cJSON_AddNumberToObject(end, "character",
                            (d->column > 0 ? d->column - 1 : 0) + d->length);

    cJSON_AddItemToObject(range, "start", start);
    cJSON_AddItemToObject(range, "end", end);
    cJSON_AddItemToObject(diagObj, "range", range);

    cJSON_AddItemToArray(diagArray, diagObj);
  }

  cJSON_AddItemToObject(params, "diagnostics", diagArray);
  cJSON_AddItemToObject(notification, "params", params);

  sendResponse(notification);
  cJSON_Delete(notification);
}

// --- THE HOVER CODEX (Phase 6) ---
static void sendHoverResponse(cJSON *id, int cursorLine, int cursorCol) {
  lspLog("[HOVER] Building documentation for line %d, col %d...", cursorLine,
         cursorCol);

  // 1. We must run the Oracle to ensure the Memory Palace is up to date!
  if (currentDocumentText != NULL) {
    lspSymbolCount = 0;
    currentScopeDepth = 0;
    targetCursorLine = cursorLine;
    isWalkingPaused = false;
    lspBlueprintCount = 0;

    Node *ast = parseSource(currentDocumentText, 1);
    if (ast != NULL) {
      analyzeNode(ast);
      freeNode(ast);
    }
  }

  cJSON *response = cJSON_CreateObject();
  cJSON_AddStringToObject(response, "jsonrpc", "2.0");
  if (id)
    cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));

  // 2. Extract the exact word under the cursor
  char targetObj[64] = {0};
  if (currentDocumentText != NULL) {
    int currentL = 1;
    char *ptr = currentDocumentText;
    while (*ptr != '\0' && currentL < cursorLine) {
      if (*ptr == '\n')
        currentL++;
      ptr++;
    }
    for (int i = 0; i < cursorCol && *ptr != '\0'; i++)
      ptr++;

    // Scan backwards to find the start of the word
    char *startWord = ptr;
    while (startWord > currentDocumentText &&
           (*(startWord - 1) == '_' ||
            (*(startWord - 1) >= 'a' && *(startWord - 1) <= 'z') ||
            (*(startWord - 1) >= 'A' && *(startWord - 1) <= 'Z') ||
            (*(startWord - 1) >= '0' && *(startWord - 1) <= '9'))) {
      startWord--;
    }

    // Scan forwards to find the end of the word
    char *endWord = ptr;
    while (*endWord == '_' || (*endWord >= 'a' && *endWord <= 'z') ||
           (*endWord >= 'A' && *endWord <= 'Z') ||
           (*endWord >= '0' && *endWord <= '9')) {
      endWord++;
    }

    int len = endWord - startWord;
    if (len > 0 && len < 63) {
      strncpy(targetObj, startWord, len);
      targetObj[len] = '\0';
    }
  }

  // 3. Look it up in the Memory Palace!
  bool found = false;
  for (int i = 0; i < lspSymbolCount; i++) {
    if (strcmp(lspSymbols[i].name, targetObj) == 0) {
      found = true;

      cJSON *result = cJSON_CreateObject();
      cJSON *contents = cJSON_CreateObject();
      cJSON_AddStringToObject(contents, "kind", "markdown");

      char markdown[4096];
      const char *typeStr = "Unknown";
      if (lspSymbols[i].type == LSP_TYPE_NUMBER)
        typeStr = "Number";
      else if (lspSymbols[i].type == LSP_TYPE_STRING)
        typeStr = "String";
      else if (lspSymbols[i].type == LSP_TYPE_LIST)
        typeStr = "List";
      else if (lspSymbols[i].type == LSP_TYPE_DICTIONARY)
        typeStr = "Dictionary";
      else if (lspSymbols[i].type == LSP_TYPE_BLUEPRINT)
        typeStr = "Blueprint";
      else if (lspSymbols[i].type == LSP_TYPE_FUNCTION)
        typeStr = "Function";
      else if (lspSymbols[i].type == LSP_TYPE_UNION)
        typeStr = "Union Type";

      // Extract docstring
      char docBuf[256];
      extractDocstring(lspSymbols[i].line, docBuf, sizeof(docBuf));
      char docSection[300] = "";
      if (docBuf[0] != '\0') {
        snprintf(docSection, sizeof(docSection), "\n\n---\n*%s*", docBuf);
      }

      if (lspSymbols[i].type == LSP_TYPE_BLUEPRINT &&
          lspSymbols[i].blueprintName[0] != '\0') {
        // It's an INSTANCE of a Blueprint! Let's fetch its properties.
        char propsStr[2048] = "";
        for (int b = 0; b < lspBlueprintCount; b++) {
          if (strcmp(lspBlueprints[b].name, lspSymbols[i].blueprintName) == 0) {
            for (int p = 0; p < lspBlueprints[b].propCount; p++) {
              strcat(propsStr, "`");
              strcat(propsStr, lspBlueprints[b].properties[p]);
              strcat(propsStr, "` ");
            }
            break;
          }
        }

        snprintf(markdown, sizeof(markdown),
                 "### MOON %s Instance\n"
                 "**Type:** `%s`\n\n"
                 "**Properties:** %s\n\n"
                 "*Scope Depth: %d*%s",
                 lspSymbols[i].blueprintName, lspSymbols[i].blueprintName,
                 propsStr, lspSymbols[i].depth, docSection);
      } else {
        // It's a normal variable
        snprintf(markdown, sizeof(markdown),
                 "### MOON Variable: `%s`\n"
                 "**Inferred Type:** `%s`\n\n"
                 "*Scope Depth: %d*%s",
                 lspSymbols[i].name, typeStr, lspSymbols[i].depth, docSection);
      }

      cJSON_AddStringToObject(contents, "value", markdown);
      cJSON_AddItemToObject(result, "contents", contents);
      cJSON_AddItemToObject(response, "result", result);
      break;
    }
  }

  if (!found) {
    cJSON_AddNullToObject(response, "result");
  }

  sendResponse(response);
  cJSON_Delete(response);
  lspLog("[HOVER] Success!");
}

static bool formatPhrasalSnippet(const char *mangled, char *label, char *snippet, char *filterText) {
  int lIdx = 0, sIdx = 0, fIdx = 0;
  bool isPhrasal = false;
  int argCount = 1;

  for (int c = 0; mangled[c] != '\0'; c++) {
    if (mangled[c] == '_') {
      label[lIdx++] = ' ';
      snippet[sIdx++] = ' ';
      if (fIdx > 0 && filterText[fIdx-1] != ' ' && fIdx < 63) filterText[fIdx++] = ' ';
      isPhrasal = true;
    } else if (mangled[c] == '$' && mangled[c+1] >= '0' && mangled[c+1] <= '9') {
      if (lIdx > 0 && label[lIdx-1] != ' ') {
        label[lIdx++] = ' ';
        snippet[sIdx++] = ' ';
      }
      
      int arity = mangled[c+1] - '0';
      c++; // Skip the number
      isPhrasal = true;
      
      if (arity > 1) {
        label[lIdx++] = '(';
        snippet[sIdx++] = '(';
      }
      
      for (int a = 0; a < arity; a++) {
        label[lIdx++] = '.';
        sIdx += snprintf(snippet + sIdx, 256 - sIdx, "${%d:arg}", argCount++);
        
        if (a < arity - 1) {
          label[lIdx++] = ',';
          label[lIdx++] = ' ';
          snippet[sIdx++] = ',';
          snippet[sIdx++] = ' ';
        }
      }
      
      if (arity > 1) {
        label[lIdx++] = ')';
        snippet[sIdx++] = ')';
      }
    } else {
      label[lIdx++] = mangled[c];
      snippet[sIdx++] = mangled[c];
      if (fIdx < 63) filterText[fIdx++] = mangled[c];
    }
  }
  
  label[lIdx] = '\0';
  snippet[sIdx] = '\0';
  filterText[fIdx] = '\0';
  
  return isPhrasal;
}

// --- THE AUTOCOMPLETE GENERATOR (UPGRADED) ---
static void sendCompletionResponse(cJSON *id, int cursorLine, int cursorCol) {
  lspLog("[COMPLETION] Building response for line %d, col %d...", cursorLine,
         cursorCol);

  // --- 1. THE ORACLE INTERCEPT ---
  if (currentDocumentText != NULL) {
    lspSymbolCount = 0;
    currentScopeDepth = 0;
    targetCursorLine = cursorLine;
    isWalkingPaused = false;
    lspBlueprintCount = 0;

    Node *ast = parseSource(currentDocumentText, 1);
    if (ast != NULL) {
      analyzeNode(ast);
      freeNode(ast);
    }
  }

  cJSON *response = cJSON_CreateObject();
  cJSON_AddStringToObject(response, "jsonrpc", "2.0");
  if (id)
    cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));
  cJSON *result = cJSON_CreateArray();

  // --- 2. THE POSSESSION SCANNER ---
  char targetObj[64] = {0};
  bool isPropertyRequest = false;

  if (currentDocumentText != NULL) {
    int currentL = 1;
    char *ptr = currentDocumentText;
    // Seek to the correct line
    while (*ptr != '\0' && currentL < cursorLine) {
      if (*ptr == '\n')
        currentL++;
      ptr++;
    }
    // Seek to the correct column
    for (int i = 0; i < cursorCol && *ptr != '\0'; i++)
      ptr++;

    // ptr is now exactly at your cursor. Let's look backwards!
    char *scan = ptr - 1;
    while (scan >= currentDocumentText && *scan == ' ')
      scan--; // Ignore trailing spaces

    // Did we find an 's' ?
    if (scan >= currentDocumentText + 1 && *scan == 's' &&
        *(scan - 1) == '\'') {
      isPropertyRequest = true;

      // Found it! Now scan further backward to grab the variable name (e.g.,
      // 'player')
      char *startWord = scan - 2;
      while (startWord >= currentDocumentText &&
             (*startWord == '_' || (*startWord >= 'a' && *startWord <= 'z') ||
              (*startWord >= 'A' && *startWord <= 'Z') ||
              (*startWord >= '0' && *startWord <= '9'))) {
        startWord--;
      }
      startWord++; // Step forward to the first valid character

      int len = (scan - 1) - startWord;
      if (len > 0 && len < 63) {
        strncpy(targetObj, startWord, len);
        targetObj[len] = '\0';
      }
    }
  }

  // --- 3. THE RESPONSE ROUTER ---
  if (isPropertyRequest) {
    lspLog("[COMPLETION] POSSESSION TRIGGER DETECTED! Target: '%s'", targetObj);

    // A. Find the variable in our symbols
    char *blueprintName = NULL;
    for (int i = 0; i < lspSymbolCount; i++) {
      if (strcmp(lspSymbols[i].name, targetObj) == 0) {
        blueprintName = lspSymbols[i].blueprintName;
        break;
      }
    }

    if (blueprintName != NULL && blueprintName[0] != '\0') {
      // B. Find its Blueprint in the Registry!
      for (int i = 0; i < lspBlueprintCount; i++) {
        if (strcmp(lspBlueprints[i].name, blueprintName) == 0) {
          lspLog("[COMPLETION] Found Blueprint '%s'. Sending %d properties.",
                 blueprintName, lspBlueprints[i].propCount);

          // C. Spit out ONLY the properties!
          for (int p = 0; p < lspBlueprints[i].propCount; p++) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "label",
                                    lspBlueprints[i].properties[p]);
            cJSON_AddNumberToObject(item, "kind", 10); // 10 = Property Icon

            char detailStr[128];
            snprintf(detailStr, sizeof(detailStr), "Property of %s",
                     blueprintName);
            cJSON_AddStringToObject(item, "detail", detailStr);
            cJSON_AddItemToArray(result, item);
          }
          break;
        }
      }
    }
  } else {
    // --- NORMAL AUTOCOMPLETE (Variables & Keywords) ---
    const char *keywords[] = {"let",       "be",     "set",    "to",
                              "add",       "show",   "give",   "if",
                              "else",      "unless", "while",  "until",
                              "read file", "write",  "append", "exists"};
    int numKeywords = sizeof(keywords) / sizeof(keywords[0]);

    for (int i = 0; i < numKeywords; i++) {
      cJSON *item = cJSON_CreateObject();
      cJSON_AddStringToObject(item, "label", keywords[i]);
      cJSON_AddNumberToObject(item, "kind", 14);
      cJSON_AddStringToObject(item, "detail", "MOON Core");
      cJSON_AddItemToArray(result, item);
    }

    // --- 3B. THE UNIFIED PHRASAL GENERATOR ---
    
    // Loop 1: VM Globals (Standard Library)
    for (int i = 0; i < vm.globals.capacity; i++) {
      Entry *entry = &vm.globals.entries[i];
      if (IS_EMPTY(entry->key) || IS_TOMB(entry->key)) continue;
      
      if (IS_NATIVE(entry->value) || IS_FUNCTION(entry->value) || (IS_OBJ(entry->value) && OBJ_TYPE(entry->value) == OBJ_MULTI_FUNCTION)) {
        char *mangled = AS_CSTRING(entry->key);
        if (mangled[0] == '_' && mangled[1] == '_') continue; // Skip internal primitives

        char label[128] = {0}; char snippet[256] = {0}; char filterText[128] = {0};
        if (formatPhrasalSnippet(mangled, label, snippet, filterText)) {
          cJSON *item = cJSON_CreateObject();
          cJSON_AddStringToObject(item, "label", label);
          cJSON_AddStringToObject(item, "filterText", filterText);
          cJSON_AddNumberToObject(item, "kind", 15); // 15 = Snippet Icon
          cJSON_AddNumberToObject(item, "insertTextFormat", 2);
          cJSON_AddStringToObject(item, "insertText", snippet);
          cJSON_AddStringToObject(item, "detail", "MOON Core Function");
          cJSON_AddItemToArray(result, item);
        }
      }
    }

    // Loop 2: User Symbols
    for (int i = 0; i < lspSymbolCount; i++) {
      // Skip Aliases (structural words registered just for Hover)
      if (lspSymbols[i].type == LSP_TYPE_FUNCTION && lspSymbols[i].blueprintName[0] != '\0') {
        continue;
      }

      cJSON *item = cJSON_CreateObject();
      
      if (lspSymbols[i].type == LSP_TYPE_FUNCTION) {
        char label[128] = {0}; char snippet[256] = {0}; char filterText[128] = {0};
        if (formatPhrasalSnippet(lspSymbols[i].name, label, snippet, filterText)) {
          cJSON_AddStringToObject(item, "label", label);
          cJSON_AddStringToObject(item, "filterText", filterText);
          cJSON_AddNumberToObject(item, "kind", 15); // 15 = Snippet
          cJSON_AddNumberToObject(item, "insertTextFormat", 2);
          cJSON_AddStringToObject(item, "insertText", snippet);
        } else {
          cJSON_AddStringToObject(item, "label", lspSymbols[i].name);
          cJSON_AddNumberToObject(item, "kind", 15); // Use Snippet even for non-phrasal to avoid `()`
        }
      } else {
        // Variables and Blueprints
        cJSON_AddStringToObject(item, "label", lspSymbols[i].name);
        int kind = (lspSymbols[i].type == LSP_TYPE_BLUEPRINT) ? 7 : 6;
        cJSON_AddNumberToObject(item, "kind", kind);
      }

      const char *typeStr = "Unknown";
      if (lspSymbols[i].type == LSP_TYPE_NUMBER) typeStr = "Number";
      else if (lspSymbols[i].type == LSP_TYPE_STRING) typeStr = "String";
      else if (lspSymbols[i].type == LSP_TYPE_LIST) typeStr = "List";
      else if (lspSymbols[i].type == LSP_TYPE_DICTIONARY) typeStr = "Dict";
      else if (lspSymbols[i].type == LSP_TYPE_BLUEPRINT) typeStr = "Blueprint";
      else if (lspSymbols[i].type == LSP_TYPE_FUNCTION) typeStr = "Function";

      char desc[128];
      if (lspSymbols[i].type != LSP_TYPE_FUNCTION && lspSymbols[i].blueprintName[0] != '\0') {
        snprintf(desc, sizeof(desc), "MOON %s (Instance of %s)", typeStr, lspSymbols[i].blueprintName);
      } else {
        snprintf(desc, sizeof(desc), "MOON %s (Depth %d)", typeStr, lspSymbols[i].depth);
      }

      cJSON_AddStringToObject(item, "detail", desc);
      cJSON_AddItemToArray(result, item);
    }
  }

  cJSON_AddItemToObject(response, "result", result);
  sendResponse(response);
  cJSON_Delete(response);
  lspLog("[COMPLETION] Success!");
}

static void sendSemanticTokens(cJSON *id, const char *uri) {
  lspLog("[SEMANTIC] Generating tokens for %s", uri);

  // 1. Build the flat array
  cJSON *response = cJSON_CreateObject();
  cJSON_AddStringToObject(response, "jsonrpc", "2.0");
  if (id)
    cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));

  cJSON *result = cJSON_CreateObject();
  cJSON *dataArray = cJSON_CreateArray();

  // 2. Sort the harvested tokens top-to-bottom, left-to-right
  qsort(semTokens, semTokenCount, sizeof(SemanticToken), compareTokens);

  // 2.5 Deduplicate: If multiple tokens exist at the exact same line/col, keep the highest priority one (which is sorted LAST)
  int uniqueCount = 0;
  for (int i = 0; i < semTokenCount; i++) {
    if (i < semTokenCount - 1 &&
        semTokens[i].line == semTokens[i+1].line &&
        semTokens[i].character == semTokens[i+1].character) {
      continue; // Skip this one, the next one is higher priority
    }
    semTokens[uniqueCount++] = semTokens[i];
  }
  semTokenCount = uniqueCount;

  // 3. The Delta Encoder
  int prevLine = 0;
  int prevChar = 0;

  for (int i = 0; i < semTokenCount; i++) {
    int deltaLine = semTokens[i].line - prevLine;
    int deltaChar = (deltaLine == 0) ? (semTokens[i].character - prevChar)
                                     : semTokens[i].character;

    cJSON_AddItemToArray(dataArray, cJSON_CreateNumber(deltaLine));
    cJSON_AddItemToArray(dataArray, cJSON_CreateNumber(deltaChar));
    cJSON_AddItemToArray(dataArray, cJSON_CreateNumber(semTokens[i].length));
    cJSON_AddItemToArray(dataArray, cJSON_CreateNumber(semTokens[i].type));
    cJSON_AddItemToArray(dataArray, cJSON_CreateNumber(semTokens[i].modifiers));

    prevLine = semTokens[i].line;
    prevChar = semTokens[i].character;
  }

  cJSON_AddItemToObject(result, "data", dataArray);
  cJSON_AddItemToObject(response, "result", result);
  sendResponse(response);
  cJSON_Delete(response);
}

// --- THE GO-TO DEFINITION TELEPORTER (Phase 2) ---
static void sendDefinitionResponse(cJSON *id, const char *uri, int cursorLine,
                                   int cursorCol) {
  lspLog("[DEFINITION] Looking up symbol at line %d, col %d...", cursorLine,
         cursorCol);

  // 1. Ensure the Palace is up to date
  if (currentDocumentText != NULL) {
    lspSymbolCount = 0;
    currentScopeDepth = 0;
    targetCursorLine = cursorLine; // Stop walking once we hit the cursor!
    isWalkingPaused = false;
    lspBlueprintCount = 0;
    Node *ast = parseSource(currentDocumentText, 1);
    if (ast != NULL) {
      analyzeNode(ast);
      freeNode(ast);
    }
  }

  cJSON *response = cJSON_CreateObject();
  cJSON_AddStringToObject(response, "jsonrpc", "2.0");
  if (id)
    cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));

  // 2. Extract the exact word under the cursor
  char targetObj[64] = {0};
  if (currentDocumentText != NULL) {
    int currentL = 1;
    char *ptr = currentDocumentText;
    while (*ptr != '\0' && currentL < cursorLine) {
      if (*ptr == '\n')
        currentL++;
      ptr++;
    }
    for (int i = 0; i < cursorCol && *ptr != '\0'; i++)
      ptr++;

    // Scan backwards
    char *startWord = ptr;
    while (startWord > currentDocumentText &&
           (*(startWord - 1) == '_' ||
            (*(startWord - 1) >= 'a' && *(startWord - 1) <= 'z') ||
            (*(startWord - 1) >= 'A' && *(startWord - 1) <= 'Z') ||
            (*(startWord - 1) >= '0' && *(startWord - 1) <= '9'))) {
      startWord--;
    }
    // Scan forwards
    char *endWord = ptr;
    while (*endWord == '_' || (*endWord >= 'a' && *endWord <= 'z') ||
           (*endWord >= 'A' && *endWord <= 'Z') ||
           (*endWord >= '0' && *endWord <= '9')) {
      endWord++;
    }

    int len = endWord - startWord;
    if (len > 0 && len < 63) {
      strncpy(targetObj, startWord, len);
      targetObj[len] = '\0';
    }
  }

  // 3. Find its birthplace in the Memory Palace!
  bool found = false;
  for (int i = 0; i < lspSymbolCount; i++) {
    if (strcmp(lspSymbols[i].name, targetObj) == 0) {
      found = true;

      // Build the LSP Location Object
      cJSON *location = cJSON_CreateObject();
      cJSON_AddStringToObject(location, "uri", uri);

      cJSON *range = cJSON_CreateObject();
      cJSON *start = cJSON_CreateObject();

      // LSP is 0-indexed, MOON is 1-indexed!
      int destLine = lspSymbols[i].line > 0 ? lspSymbols[i].line - 1 : 0;
      int destCol = lspSymbols[i].column > 0 ? lspSymbols[i].column - 1 : 0;

      cJSON_AddNumberToObject(start, "line", destLine);
      cJSON_AddNumberToObject(start, "character", destCol);

      cJSON *end = cJSON_CreateObject();
      cJSON_AddNumberToObject(end, "line", destLine);
      cJSON_AddNumberToObject(end, "character",
                              destCol + strlen(lspSymbols[i].name));

      cJSON_AddItemToObject(range, "start", start);
      cJSON_AddItemToObject(range, "end", end);
      cJSON_AddItemToObject(location, "range", range);

      cJSON_AddItemToObject(response, "result", location);
      lspLog("[DEFINITION] Success! Teleporting to Line %d", destLine + 1);
      break;
    }
  }

  if (!found) {
    lspLog("[DEFINITION] Target '%s' not found.", targetObj);
    cJSON_AddNullToObject(response, "result");
  }

  sendResponse(response);
  cJSON_Delete(response);
}

// --- THE SIGNATURE HELP ENGINE (Phase 3) ---
static void sendSignatureHelpResponse(cJSON *id, int cursorLine,
                                      int cursorCol) {
  cJSON *response = cJSON_CreateObject();
  cJSON_AddStringToObject(response, "jsonrpc", "2.0");
  if (id)
    cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));

  cJSON *result = cJSON_CreateObject();
  cJSON *signatures = cJSON_CreateArray();
  int activeParameter = 0;

  if (currentDocumentText != NULL) {
    int currentL = 1;
    char *ptr = currentDocumentText;
    while (*ptr != '\0' && currentL < cursorLine) {
      if (*ptr == '\n')
        currentL++;
      ptr++;
    }
    for (int i = 0; i < cursorCol && *ptr != '\0'; i++)
      ptr++;

    // 1. Scan backwards to find the '(' or 'with'
    char *scan = ptr - 1;
    while (scan >= currentDocumentText && *scan != '(' &&
           strncmp(scan, "with", 4) != 0 && *scan != '\n') {
      if (*scan == ',')
        activeParameter++; // Count commas to highlight the active parameter!
      scan--;
    }

    if (scan >= currentDocumentText &&
        (*scan == '(' || strncmp(scan, "with", 4) == 0)) {
      char targetObj[64] = {0};
      char *endWord = scan - 1;
      while (endWord >= currentDocumentText &&
             (*endWord == ' ' || *endWord == '\t'))
        endWord--;
      char *startWord = endWord;
      while (startWord > currentDocumentText &&
             (*(startWord - 1) == '_' ||
              (*(startWord - 1) >= 'a' && *(startWord - 1) <= 'z') ||
              (*(startWord - 1) >= 'A' && *(startWord - 1) <= 'Z') ||
              (*(startWord - 1) >= '0' && *(startWord - 1) <= '9'))) {
        startWord--;
      }

      int len = endWord - startWord + 1;
      if (len > 0 && len < 63) {
        strncpy(targetObj, startWord, len);
        targetObj[len] = '\0';
      }

      // 2. Look it up in the Registry/Palace
      // char docBuf[256];
      // bool found = false;

      // Is it a Blueprint? (Instantiating with 'with')
      for (int i = 0; i < lspBlueprintCount; i++) {
        if (strcmp(lspBlueprints[i].name, targetObj) == 0) {
          // found = true;
          cJSON *sig = cJSON_CreateObject();
          cJSON_AddStringToObject(sig, "label", targetObj);

          cJSON *params = cJSON_CreateArray();
          for (int p = 0; p < lspBlueprints[i].propCount; p++) {
            cJSON *param = cJSON_CreateObject();
            char pLabel[128];
            snprintf(pLabel, sizeof(pLabel), "%s: Any",
                     lspBlueprints[i].properties[p]);
            cJSON_AddStringToObject(param, "label", pLabel);
            cJSON_AddItemToArray(params, param);
          }
          cJSON_AddItemToObject(sig, "parameters", params);
          cJSON_AddItemToArray(signatures, sig);
          break;
        }
      }

      // If not a blueprint, maybe a function? (We will expand this later when
      // you add signature arrays to lspSymbols) For now, it gracefully handles
      // Blueprints flawlessly!
    }
  }

  cJSON_AddItemToObject(result, "signatures", signatures);
  cJSON_AddNumberToObject(result, "activeSignature", 0);
  cJSON_AddNumberToObject(result, "activeParameter", activeParameter);
  cJSON_AddItemToObject(response, "result", result);
  sendResponse(response);
  cJSON_Delete(response);
}

// ==========================================
// PHASE 4: THE AUTO-FORMATTER
// ==========================================

// 1. The Dynamic Array (String Builder)
typedef struct {
  char *chars;
  int length;
  int capacity;
} FormatterBuffer;

static void initFormatterBuffer(FormatterBuffer *buf) {
  buf->capacity = 1024;
  buf->length = 0;
  buf->chars = malloc(buf->capacity);
  buf->chars[0] = '\0';
}

static void writeFormat(FormatterBuffer *buf, const char *str, int len) {
  if (buf->capacity < buf->length + len + 1) {
    buf->capacity = (buf->length + len + 1) * 2;
    buf->chars = realloc(buf->chars, buf->capacity);
  }
  memcpy(buf->chars + buf->length, str, len);
  buf->length += len;
  buf->chars[buf->length] = '\0';
}

// Helper to identify operators that need spaces around them
static bool isSpacedOperator(TokenType type) {
  return type == TOKEN_EQUAL || type == TOKEN_EQUAL_EQUAL ||
         type == TOKEN_PLUS || type == TOKEN_MINUS || type == TOKEN_STAR ||
         type == TOKEN_SLASH || type == TOKEN_MOD || type == TOKEN_BANG_EQUAL ||
         type == TOKEN_GREATER || type == TOKEN_GREATER_EQUAL ||
         type == TOKEN_LESS || type == TOKEN_LESS_EQUAL || type == TOKEN_BE ||
         type == TOKEN_TO || type == TOKEN_IS || type == TOKEN_AND ||
         type == TOKEN_OR || type == TOKEN_AS || type == TOKEN_UPDATE;
}

// 2. The Smart Context-Aware Formatter
char *formatSource(const char *source) {
  FormatterBuffer buf;
  initFormatterBuffer(&buf);

  Scanner previousScanner = scanner;
  initScanner(source, 1);
  scanner.preserveComments = true; // <--- THE COMMENT SHIELD

  int indentLevel = 0;
  int braceDepth = 0;
  int bracketDepth = 0;
  int parenDepth = 0;

  bool needsIndent = false;
  int consecutiveNewlines = 0;
  bool justForcedNewline = false;

  Token prevToken = {.type = TOKEN_EOF};

  bool expectsBlockColon = false;
  int blockColonBraceDepth = 0;
  int blockColonParenDepth = 0;
  bool inTypeBlock = false;
  int inlineIndentLevel = 0;
  bool inForDecl = false;

  bool bracketIsSubscript[256] = {false};

  for (;;) {
    Token token = scanToken();
    if (token.type == TOKEN_EOF)
      break;

    // --- 0. PRE-TOKEN DEPTH & STATE TRACKING ---
    // SAFELY bound depths to prevent Segfaults during live-typing!
    if (token.type == TOKEN_LEFT_BRACE)
      braceDepth++;
    if (token.type == TOKEN_RIGHT_BRACE && braceDepth > 0)
      braceDepth--;
    if (token.type == TOKEN_LEFT_PAREN)
      parenDepth++;
    if (token.type == TOKEN_RIGHT_PAREN && parenDepth > 0)
      parenDepth--;

    if (token.type == TOKEN_LEFT_BRACKET) {
      bool isSub = (prevToken.type == TOKEN_IDENTIFIER ||
                    prevToken.type == TOKEN_STRING ||
                    prevToken.type == TOKEN_RIGHT_BRACKET ||
                    prevToken.type == TOKEN_RIGHT_PAREN ||
                    prevToken.type == TOKEN_POSSESSIVE);
      if (isSub && token.start > prevToken.start + prevToken.length) {
        isSub = false; // It had a space, so it's a phrasal call, not a subscript!
      }
      if (bracketDepth >= 0 && bracketDepth < 256) {
        bracketIsSubscript[bracketDepth] = isSub;
      }
      bracketDepth++;
    }

    bool currentBracketIsSubscript = (bracketDepth > 0 && bracketDepth <= 256)
                                         ? bracketIsSubscript[bracketDepth - 1]
                                         : false;

    if (token.type == TOKEN_RIGHT_BRACKET) {
      if (bracketDepth > 0)
        bracketDepth--;
    }

    if (token.type == TOKEN_TYPE || token.type == TOKEN_LET ||
        token.type == TOKEN_IF || token.type == TOKEN_UNLESS ||
        token.type == TOKEN_WHILE || token.type == TOKEN_UNTIL ||
        token.type == TOKEN_FOR || token.type == TOKEN_ELSE) {
      if (token.type == TOKEN_IF) {
        lspLog("Token IF hit! indentLevel: %d\n", indentLevel);
      }
      expectsBlockColon = true;
      blockColonBraceDepth = braceDepth;
      blockColonParenDepth = parenDepth;
    } else if (token.type == TOKEN_BE || token.type == TOKEN_WITH || token.type == TOKEN_KEEP) {
      expectsBlockColon = false;
    }

    if (token.type == TOKEN_TYPE)
      inTypeBlock = true;
    if (token.type == TOKEN_END && bracketDepth == 0)
      inTypeBlock = false;

    bool isBlockCloser =
        (token.type == TOKEN_RIGHT_BRACE ||
         (token.type == TOKEN_END && (bracketDepth == 0 || needsIndent)));

    // Ternary 'else' shouldn't force a newline! Only block 'else'.
    if (token.type == TOKEN_ELSE) {
      Scanner temp = scanner;
      Token next = scanToken();
      scanner = temp;
      
      if (next.type == TOKEN_COLON || next.type == TOKEN_IF || 
          next.type == TOKEN_UNLESS || next.type == TOKEN_NEWLINE || next.type == TOKEN_EOF) {
        isBlockCloser = true;
      }
    }

    // --- 1. HANDLE NEWLINES ---
    if (token.type == TOKEN_NEWLINE) {
      if (expectsBlockColon) {
        inlineIndentLevel++;
      } else {
        inlineIndentLevel = 0;
      }
      expectsBlockColon = false;

      if (justForcedNewline) {
        justForcedNewline = false;
        continue;
      }
      consecutiveNewlines++;
      if (consecutiveNewlines <= 2) {
        writeFormat(&buf, "\n", 1);
      }
      needsIndent = true;
      prevToken = token;
      continue;
    }

    justForcedNewline = false;
    bool isStartOfLine = needsIndent;

    // --- 2. FORCE NEWLINES BEFORE CLOSERS ---
    if (isBlockCloser && !isStartOfLine && prevToken.type != TOKEN_COLON) {
      writeFormat(&buf, "\n", 1);
      isStartOfLine = true;
      needsIndent = true;
      consecutiveNewlines = 1;
    }

    // --- 3. DECREASE INDENT ---
    if (isBlockCloser || token.type == TOKEN_RIGHT_BRACKET) {
      if (indentLevel > 0)
        indentLevel--;
      inlineIndentLevel = 0;
    }

    // --- 4. APPLY 2-SPACE INDENTATION ---
    if (isStartOfLine) {
      int effectiveIndent = indentLevel + inlineIndentLevel;
      for (int i = 0; i < effectiveIndent * 2; i++) {
        writeFormat(&buf, " ", 1);
      }
      needsIndent = false;
    }

    if (token.type == TOKEN_FOR) inForDecl = true;
    if (token.type == TOKEN_IN) inForDecl = false;

    // --- 5. SMART SPACING MATRIX ---
    if (!isStartOfLine && prevToken.type != TOKEN_EOF &&
        prevToken.type != TOKEN_LEFT_PAREN) {
      bool spaceNeeded = false;

      bool prevIsWord =
          (prevToken.type >= TOKEN_ADD || prevToken.type == TOKEN_IDENTIFIER ||
           prevToken.type == TOKEN_NUMBER || prevToken.type == TOKEN_FALSE ||
           prevToken.type == TOKEN_TRUE || prevToken.type == TOKEN_NIL ||
           prevToken.type == TOKEN_STRING || prevToken.type == TOKEN_STRING_OPEN ||
           prevToken.type == TOKEN_STRING_CLOSE);
      bool currIsWord =
          (token.type >= TOKEN_ADD || token.type == TOKEN_IDENTIFIER ||
           token.type == TOKEN_NUMBER || token.type == TOKEN_FALSE ||
           token.type == TOKEN_TRUE || token.type == TOKEN_NIL ||
           token.type == TOKEN_STRING || token.type == TOKEN_STRING_OPEN ||
           token.type == TOKEN_STRING_CLOSE);
      if (prevIsWord && currIsWord)
        spaceNeeded = true;

      if (isSpacedOperator(token.type) || isSpacedOperator(prevToken.type))
        spaceNeeded = true;

      if (token.type == TOKEN_LEFT_BRACE)
        spaceNeeded = true;
      if (token.type == TOKEN_LEFT_BRACKET && !currentBracketIsSubscript)
        spaceNeeded = true;
      if (token.type == TOKEN_RIGHT_BRACKET && !currentBracketIsSubscript)
        spaceNeeded = true;

      if (prevToken.type == TOKEN_COMMA)
        spaceNeeded = true;
      if (prevToken.type == TOKEN_POSSESSIVE)
        spaceNeeded = true;
      if (prevToken.type == TOKEN_COLON)
        spaceNeeded = true;
      if (prevToken.type == TOKEN_LEFT_BRACKET && !currentBracketIsSubscript)
        spaceNeeded = true;

      if (prevToken.type == TOKEN_RIGHT_PAREN && currIsWord)
        spaceNeeded = true;
      if (token.type == TOKEN_LEFT_PAREN && expectsBlockColon)
        spaceNeeded = true;

      // SPACE BEFORE COMMENTS!
      if (token.type == TOKEN_COMMENT)
        spaceNeeded = true;

      if (token.type == TOKEN_LEFT_PAREN)
        spaceNeeded = true;

      if (token.type == TOKEN_COMMA || token.type == TOKEN_COLON ||
          (token.type == TOKEN_RIGHT_PAREN && !currIsWord) ||
          (token.type == TOKEN_RIGHT_BRACKET && currentBracketIsSubscript) ||
          token.type == TOKEN_STRING_MIDDLE || token.type == TOKEN_STRING_CLOSE ||
          prevToken.type == TOKEN_STRING_OPEN || prevToken.type == TOKEN_STRING_MIDDLE) {
        spaceNeeded = false;
      }

      if (spaceNeeded)
        writeFormat(&buf, " ", 1);
    }

    // --- 6. PRINT THE TOKEN ---
    if (token.type == TOKEN_COLON) {
      lspLog("Token COLON hit! expectsBlockColon: %d, braceDepth: %d, blockColonBraceDepth: %d\n", expectsBlockColon, braceDepth, blockColonBraceDepth);
      if (expectsBlockColon && braceDepth == blockColonBraceDepth &&
          parenDepth == blockColonParenDepth) {

        const char *p = token.start + 1;
        while (*p == ' ' || *p == '\t' || *p == '\r')
          p++;

        if (*p == '#') {
          // Inline comment drop!
          writeFormat(&buf, ":", 1);
          indentLevel++;
          expectsBlockColon = false;
        } else {
          // Normal block drop!
          writeFormat(&buf, ":\n", 2);
          indentLevel++;
          needsIndent = true;
          consecutiveNewlines = 1;
          justForcedNewline = true;
          expectsBlockColon = false;
        }
      } else {
        writeFormat(&buf, ":", 1);
      }
    } else if (token.type == TOKEN_LEFT_BRACE) {
      writeFormat(&buf, "{\n", 2);
      indentLevel++;
      needsIndent = true;
      consecutiveNewlines = 1;
      justForcedNewline = true;
    } else if (token.type == TOKEN_COMMENT) {
      writeFormat(&buf, token.start, token.length);
      consecutiveNewlines = 0;
    } else {
      writeFormat(&buf, token.start, token.length);
      consecutiveNewlines = 0;
    }

    // --- 7. POST-TOKEN ADJUSTMENTS ---
    if (token.type == TOKEN_LEFT_BRACKET) {
      indentLevel++;
    }
    if (token.type == TOKEN_COMMA && (braceDepth > 0 || inTypeBlock) && !inForDecl) {
      writeFormat(&buf, "\n", 1);
      needsIndent = true;
      consecutiveNewlines = 1;
      justForcedNewline = true;
    }

    prevToken = token;
  }

  scanner = previousScanner;
  return buf.chars;
}

static void sendFormattingResponse(cJSON *id, const char *uri) {
  if (currentDocumentText == NULL)
    return;
  lspLog("[FORMATTER] Formatting document: %s", uri);

  // 1. Generate the perfect string
  char *formattedCode = formatSource(currentDocumentText);
  lspLog("FORMATTED CODE:\n%s\n---END---\n", formattedCode);

  // 2. Build the JSON Payload
  cJSON *response = cJSON_CreateObject();
  cJSON_AddStringToObject(response, "jsonrpc", "2.0");
  if (id)
    cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));

  cJSON *result = cJSON_CreateArray();
  cJSON *textEdit = cJSON_CreateObject();

  // 3. Set the Range (Replace everything from Line 0 to Line 999999)
  cJSON *range = cJSON_CreateObject();
  cJSON *start = cJSON_CreateObject();
  cJSON_AddNumberToObject(start, "line", 0);
  cJSON_AddNumberToObject(start, "character", 0);
  cJSON *end = cJSON_CreateObject();
  cJSON_AddNumberToObject(end, "line", 999999);
  cJSON_AddNumberToObject(end, "character", 0);
  cJSON_AddItemToObject(range, "start", start);
  cJSON_AddItemToObject(range, "end", end);

  // 4. Attach the new text
  cJSON_AddItemToObject(textEdit, "range", range);
  cJSON_AddStringToObject(textEdit, "newText", formattedCode);
  cJSON_AddItemToArray(result, textEdit);

  cJSON_AddItemToObject(response, "result", result);
  sendResponse(response);

  cJSON_Delete(response);
  free(formattedCode); // Safe dynamic array freed!
}

// --- THE INFINITE LISTENER ---
void startLanguageServer() {
  char buffer[1024];

  // The Language Server NEVER exits until the editor kills it
  while (1) {
    int contentLength = 0;

    // 1. Parse the HTTP-style Headers
    while (fgets(buffer, sizeof(buffer), stdin)) {
      // Check for the Content-Length header
      if (strncmp(buffer, "Content-Length: ", 16) == 0) {
        contentLength = atoi(buffer + 16);
      }
      // A blank line (\r\n) means the headers are done, payload is next!
      if (strcmp(buffer, "\r\n") == 0) {
        break;
      }
    }

    // If stdin closes (editor disconnected), shut down the VM safely
    if (contentLength == 0)
      break;

    // 2. Read the exact JSON payload
    char *payload = malloc(contentLength + 1);
    size_t bytesRead = fread(payload, 1, contentLength, stdin);
    payload[bytesRead] = '\0';

    // --- PROBE 1: Did we get the raw text? ---
    lspLog("\n[INCOMING] Content-Length: %d", contentLength);
    lspLog("[PAYLOAD] %s", payload);

    // 3. Parse the JSON using cJSON
    cJSON *request = cJSON_Parse(payload);

    // --- PROBE 2: Did the parser crash? ---
    if (request == NULL) {
      lspLog("[ERROR] cJSON completely failed to parse the payload!");
      const char *error_ptr = cJSON_GetErrorPtr();
      if (error_ptr != NULL)
        lspLog("[ERROR POSITION] %s", error_ptr);
    } else {
      cJSON *method = cJSON_GetObjectItem(request, "method");
      cJSON *id = cJSON_GetObjectItem(request, "id");

      if (method != NULL && cJSON_IsString(method)) {
        // --- PROBE 3: What method did we identify? ---
        lspLog("[METHOD FOUND] %s", method->valuestring);

        // 1. The Handshake
        if (strcmp(method->valuestring, "initialize") == 0) {
          cJSON *response = cJSON_CreateObject();
          cJSON_AddStringToObject(response, "jsonrpc", "2.0");
          if (id)
            cJSON_AddItemReferenceToObject(response, "id", id);

          cJSON *result = cJSON_CreateObject();
          cJSON *capabilities = cJSON_CreateObject();

          cJSON_AddNumberToObject(capabilities, "textDocumentSync", 1);

          // --- NEW: TELL NEOVIM WE SUPPORT HOVER! ---
          cJSON_AddBoolToObject(capabilities, "hoverProvider", cJSON_True);

          cJSON_AddBoolToObject(capabilities, "definitionProvider", cJSON_True);
          cJSON_AddBoolToObject(capabilities, "documentFormattingProvider",
                                cJSON_True);

          // --- NEW: SIGNATURE HELP ---
          cJSON *sigHelp = cJSON_CreateObject();
          cJSON *sigTriggerChars = cJSON_CreateArray();
          cJSON_AddItemToArray(sigTriggerChars, cJSON_CreateString("("));
          cJSON_AddItemToArray(
              sigTriggerChars,
              cJSON_CreateString(" ")); // Triggers after 'with '
          cJSON_AddItemToObject(sigHelp, "triggerCharacters", sigTriggerChars);
          cJSON_AddItemToObject(capabilities, "signatureHelpProvider", sigHelp);

          cJSON *completionProvider = cJSON_CreateObject();
          cJSON_AddBoolToObject(completionProvider, "resolveProvider",
                                cJSON_False);

          // --- NEW: THE POSSESSION TRIGGER ---
          // Tell NeoVim to ask for completions immediately when the user hits
          // the spacebar!
          cJSON *triggerChars = cJSON_CreateArray();
          cJSON_AddItemToArray(triggerChars, cJSON_CreateString(" "));
          cJSON_AddItemToObject(completionProvider, "triggerCharacters",
                                triggerChars);

          cJSON_AddItemToObject(capabilities, "completionProvider",
                                completionProvider);

          // --- NEW: THE SEMANTIC TOKEN LEGEND ---
          cJSON *semanticTokens = cJSON_CreateObject();
          cJSON *legend = cJSON_CreateObject();
          cJSON *tokenTypes = cJSON_CreateArray();
          // Map index 0-21. Must match standard LSP specification!
          const char *types[] = {
              "namespace", "type",     "class",         "enum",
              "interface", "struct",   "typeParameter", "parameter",
              "variable",  "property", "enumMember",    "event",
              "function",  "method",   "macro",         "keyword",
              "modifier",  "comment",  "string",        "number",
              "regexp",    "operator"};
          for (int i = 0; i < 22; i++) {
            cJSON_AddItemToArray(tokenTypes, cJSON_CreateString(types[i]));
          }
          cJSON_AddItemToObject(legend, "tokenTypes", tokenTypes);
          cJSON_AddItemToObject(legend, "tokenModifiers", cJSON_CreateArray());
          cJSON_AddItemToObject(semanticTokens, "legend", legend);
          cJSON_AddBoolToObject(semanticTokens, "full", cJSON_True);
          cJSON_AddItemToObject(capabilities, "semanticTokensProvider",
                                semanticTokens);
          // ---------------------------------------

          cJSON_AddItemToObject(result, "capabilities", capabilities);
          cJSON_AddItemToObject(response, "result", result);

          sendResponse(response);
          cJSON_Delete(response);
        }
        // 2. The Live Keystrokes (Diagnostics)
        else if (strcmp(method->valuestring, "textDocument/didOpen") == 0 ||
                 strcmp(method->valuestring, "textDocument/didChange") == 0) {

          cJSON *params = cJSON_GetObjectItem(request, "params");
          cJSON *textDocument = cJSON_GetObjectItem(params, "textDocument");
          cJSON *uri = cJSON_GetObjectItem(textDocument, "uri");

          cJSON *textVal = NULL;
          if (strcmp(method->valuestring, "textDocument/didChange") == 0) {
            cJSON *changes = cJSON_GetObjectItem(params, "contentChanges");
            if (cJSON_IsArray(changes)) {
              textVal =
                  cJSON_GetObjectItem(cJSON_GetArrayItem(changes, 0), "text");
            }
          } else {
            textVal = cJSON_GetObjectItem(textDocument, "text");
          }

          if (uri && textVal && cJSON_IsString(textVal)) {
            lspDiagnosticCount = 0;

            // CACHE THE TEXT FOR THE ORACLE!
            if (currentDocumentText)
              free(currentDocumentText);
            currentDocumentText = malloc(strlen(textVal->valuestring) + 1);
            strcpy(currentDocumentText, textVal->valuestring);

            // We run a full pass for Diagnostics (no freeze-frame)
            targetCursorLine = -1;
            isWalkingPaused = false;
            lspSymbolCount = 0;
            currentScopeDepth = 0;
            lspBlueprintCount = 0; // <--- ADD THIS HERE!

            Node *ast = parseSource(textVal->valuestring, 1);
            if (ast != NULL) {
              analyzeNode(ast);
              freeNode(ast);
            }
            publishDiagnostics(uri->valuestring);
          }
        }
        // 3. The Autocomplete Request
        else if (strcmp(method->valuestring, "textDocument/completion") == 0) {
          cJSON *params = cJSON_GetObjectItem(request, "params");
          cJSON *position = cJSON_GetObjectItem(params, "position");

          int cursorLine = 0;
          int cursorCol = 0; // NEW: Track the column!
          if (position) {
            cJSON *lineItem = cJSON_GetObjectItem(position, "line");
            cJSON *colItem = cJSON_GetObjectItem(position, "character");
            if (lineItem)
              cursorLine = lineItem->valueint + 1;
            if (colItem)
              cursorCol = colItem->valueint; // Grab the exact byte index
          }

          // Pass BOTH to the generator!
          sendCompletionResponse(id, cursorLine, cursorCol);
        }

        // 4. The Autocomplete Hover Details (NEW!)
        else if (strcmp(method->valuestring, "completionItem/resolve") == 0) {
          cJSON *params = cJSON_GetObjectItem(request, "params");
          cJSON *response = cJSON_CreateObject();
          cJSON_AddStringToObject(response, "jsonrpc", "2.0");
          if (id)
            cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));

          // Just bounce the exact same item back to NeoVim to satisfy the
          // request!
          if (params)
            cJSON_AddItemToObject(response, "result",
                                  cJSON_Duplicate(params, 1));

          sendResponse(response);
          cJSON_Delete(response);
        }

        // 5. The Hover Request (Phase 6)
        else if (strcmp(method->valuestring, "textDocument/hover") == 0) {
          cJSON *params = cJSON_GetObjectItem(request, "params");
          cJSON *position = cJSON_GetObjectItem(params, "position");

          int cursorLine = 0;
          int cursorCol = 0;
          if (position) {
            cJSON *lineItem = cJSON_GetObjectItem(position, "line");
            cJSON *colItem = cJSON_GetObjectItem(position, "character");
            if (lineItem)
              cursorLine = lineItem->valueint + 1;
            if (colItem)
              cursorCol = colItem->valueint;
          }

          sendHoverResponse(id, cursorLine, cursorCol);
        }

        // 6. The Semantic Token Request
        else if (strcmp(method->valuestring,
                        "textDocument/semanticTokens/full") == 0) {
          cJSON *params = cJSON_GetObjectItem(request, "params");
          cJSON *textDoc = cJSON_GetObjectItem(params, "textDocument");
          cJSON *uri = cJSON_GetObjectItem(textDoc, "uri");

          // Wipe the token array clean
          semTokenCount = 0;

          // Re-run the AST Harvester if we have code!
          if (currentDocumentText != NULL) {
            // --- 1. RAPID LEXICAL PASS ---
            // Catch all raw tokens (comments, strings, numbers, keywords) before the AST!
            initScanner(currentDocumentText, 1);
            scanner.preserveComments = true;
            Token t;
            while ((t = scanToken()).type != TOKEN_EOF) {
              if (t.type == TOKEN_COMMENT) {
                addMultilineSemToken(t, 17, 0); // Comment (Priority 0)
              } else if (t.type == TOKEN_STRING || t.type == TOKEN_STRING_OPEN || 
                         t.type == TOKEN_STRING_MIDDLE || t.type == TOKEN_STRING_CLOSE) {
                addMultilineSemToken(t, 18, 0); // String (Priority 0)
              } else if (t.type == TOKEN_NUMBER || t.type == TOKEN_TRUE || t.type == TOKEN_FALSE) {
                addSemToken(t.line, t.column, t.length, 19, 0); // Number/Boolean (Priority 0)
              } else if (t.type >= TOKEN_ADD && t.type <= TOKEN_WITH) {
                addSemToken(t.line, t.column, t.length, 15, 0); // Keyword (Priority 0)
              }
            }
            scanner.preserveComments = false; // Reset for the real parser

            // --- 2. AST HARVESTER PASS ---
            lspSymbolCount = 0;
            currentScopeDepth = 0;
            targetCursorLine = -1; // We need the FULL document!
            isWalkingPaused = false;
            lspBlueprintCount = 0;
            Node *ast = parseSource(currentDocumentText, 1);
            if (ast != NULL) {
              analyzeNode(ast);
              freeNode(ast);
            }
          }

          sendSemanticTokens(id, uri ? uri->valuestring : "unknown");
        }

        // 7. The Go-To Definition Request (Phase 2)
        else if (strcmp(method->valuestring, "textDocument/definition") == 0) {
          cJSON *params = cJSON_GetObjectItem(request, "params");
          cJSON *textDoc = cJSON_GetObjectItem(params, "textDocument");
          cJSON *uri = cJSON_GetObjectItem(textDoc, "uri");
          cJSON *position = cJSON_GetObjectItem(params, "position");

          int cursorLine = 0;
          int cursorCol = 0;
          if (position) {
            cJSON *lineItem = cJSON_GetObjectItem(position, "line");
            cJSON *colItem = cJSON_GetObjectItem(position, "character");
            if (lineItem)
              cursorLine = lineItem->valueint + 1;
            if (colItem)
              cursorCol = colItem->valueint;
          }

          sendDefinitionResponse(id, uri ? uri->valuestring : "unknown",
                                 cursorLine, cursorCol);
        }

        // 8. The Signature Help Request (Phase 3)
        else if (strcmp(method->valuestring, "textDocument/signatureHelp") ==
                 0) {
          cJSON *params = cJSON_GetObjectItem(request, "params");
          cJSON *position = cJSON_GetObjectItem(params, "position");

          int cursorLine = 0;
          int cursorCol = 0;
          if (position) {
            cJSON *lineItem = cJSON_GetObjectItem(position, "line");
            cJSON *colItem = cJSON_GetObjectItem(position, "character");
            if (lineItem)
              cursorLine = lineItem->valueint + 1;
            if (colItem)
              cursorCol = colItem->valueint;
          }

          sendSignatureHelpResponse(id, cursorLine, cursorCol);
        }

        // 9. The Formatting Request (Phase 4)
        else if (strcmp(method->valuestring, "textDocument/formatting") == 0) {
          cJSON *params = cJSON_GetObjectItem(request, "params");
          cJSON *textDoc = cJSON_GetObjectItem(params, "textDocument");
          cJSON *uri = cJSON_GetObjectItem(textDoc, "uri");

          sendFormattingResponse(id, uri ? uri->valuestring : "unknown");
        }
      }
      cJSON_Delete(request);
    }

    free(payload);
  }
}
