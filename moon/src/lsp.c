#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "cJSON.h"
#include "error.h"
#include "lsp.h"
#include "parser.h"
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
  LSP_TYPE_BLUEPRINT
} LspSymbolType;

// --- THE SPELLCHECKING ORACLE (For the LSP) ---
static int levenshteinDistanceLsp(const char *s1, const char *s2) {
  int len1 = strlen(s1);
  int len2 = strlen(s2);
  int *column = malloc((len1 + 1) * sizeof(int));
  for (int i = 0; i <= len1; i++)
    column[i] = i;

  for (int x = 1; x <= len2; x++) {
    int lastDiagonal = column[0]; // IMPORTANT: Save before overwriting!
    column[0] = x;
    for (int y = 1; y <= len1; y++) {
      int oldDiagonal = column[y];
      int cost = (s1[y - 1] == s2[x - 1]) ? 0 : 1;

      int min = column[y] + 1; // Deletion
      if (column[y - 1] + 1 < min)
        min = column[y - 1] + 1; // Insertion
      if (lastDiagonal + cost < min)
        min = lastDiagonal + cost; // Substitution

      column[y] = min; // Correctly update the current cell
      lastDiagonal = oldDiagonal;
    }
  }
  int result = column[len1];
  free(column);
  return result;
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
} LspSymbol;

// 3. The Memory Palace (Storage)
static LspSymbol lspSymbols[500];
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

// --- THE MEMORY MANAGERS ---

// 1. Adds a variable to the current room
static void registerSymbol(const char *name, LspSymbolType type,
                           const char *blueprintName) {
  if (lspSymbolCount >= 500)
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

  // Save the blueprint identity if it has one!
  if (blueprintName != NULL) {
    strncpy(lspSymbols[lspSymbolCount].blueprintName, blueprintName, 63);
    lspSymbols[lspSymbolCount].blueprintName[63] = '\0';
  } else {
    lspSymbols[lspSymbolCount].blueprintName[0] = '\0';
  }

  lspLog("[BRAIN] Registered '%s' (Type %d, Blueprint: '%s') at depth %d", name,
         type, blueprintName ? blueprintName : "None", currentScopeDepth);
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
    registerSymbol(funcName, LSP_TYPE_UNKNOWN, NULL);

    // 2. Go one level deeper for parameters and body!
    currentScopeDepth++;
    for (int i = 0; i < node->as.function.paramCount; i++) {
      char paramName[64];
      int pLen = node->as.function.parameters[i].length < 63
                     ? node->as.function.parameters[i].length
                     : 63;
      strncpy(paramName, node->as.function.parameters[i].start, pLen);
      paramName[pLen] = '\0';
      registerSymbol(paramName, LSP_TYPE_UNKNOWN, NULL);
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

    registerSymbol(iterName, LSP_TYPE_UNKNOWN, NULL);
    analyzeNode(node->as.forStmt.sequence);
    analyzeNode(node->as.forStmt.body);

    // Do not pop if the cursor is inside the loop!
    if (!isWalkingPaused)
      popScope();
    break;
  }

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
        }

        analyzeNode(expr);
      }

      char varName[64];
      int len =
          node->as.let.names[i].length < 63 ? node->as.let.names[i].length : 63;
      strncpy(varName, node->as.let.names[i].start, len);
      varName[len] = '\0';

      registerSymbol(varName, guessedType, bpName);
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

    break;
  }

  case NODE_TYPE_DECL: {
    char typeName[64];
    int len =
        node->as.typeDecl.name.length < 63 ? node->as.typeDecl.name.length : 63;
    strncpy(typeName, node->as.typeDecl.name.start, len);
    typeName[len] = '\0';

    registerSymbol(typeName, LSP_TYPE_BLUEPRINT, NULL);

    // SAVE THE PROPERTIES TO THE REGISTRY!
    registerBlueprint(typeName, node->as.typeDecl.propertyNames,
                      node->as.typeDecl.count);

    for (int i = 0; i < node->as.typeDecl.count; i++)
      analyzeNode(node->as.typeDecl.defaultValues[i]);
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

  case NODE_CALL:
    analyzeNode(node->as.call.callee);
    for (int i = 0; i < node->as.call.argCount; i++)
      analyzeNode(node->as.call.arguments[i]);
    break;

  case NODE_PHRASAL_CALL:
    for (int i = 0; i < node->as.phrasalCall.argCount; i++)
      analyzeNode(node->as.phrasalCall.arguments[i]);
    break;

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
    for (int i = 0; i < node->as.instantiate.count; i++)
      analyzeNode(node->as.instantiate.values[i]);
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

    Node *ast = parseSource(currentDocumentText);
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

      char markdown[512];
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

      if (lspSymbols[i].type == LSP_TYPE_BLUEPRINT &&
          lspSymbols[i].blueprintName[0] != '\0') {
        // It's an INSTANCE of a Blueprint! Let's fetch its properties.
        char propsStr[256] = "";
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
                 "*Scope Depth: %d*",
                 lspSymbols[i].blueprintName, lspSymbols[i].blueprintName,
                 propsStr, lspSymbols[i].depth);
      } else {
        // It's a normal variable
        snprintf(markdown, sizeof(markdown),
                 "### MOON Variable: `%s`\n"
                 "**Inferred Type:** `%s`\n\n"
                 "*Scope Depth: %d*",
                 lspSymbols[i].name, typeStr, lspSymbols[i].depth);
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

    Node *ast = parseSource(currentDocumentText);
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

    for (int i = 0; i < lspSymbolCount; i++) {
      cJSON *item = cJSON_CreateObject();
      cJSON_AddStringToObject(item, "label", lspSymbols[i].name);

      int kind = 6;
      if (lspSymbols[i].type == LSP_TYPE_BLUEPRINT)
        kind = 7;
      else if (lspSymbols[i].type == LSP_TYPE_FUNCTION)
        kind = 3;

      cJSON_AddNumberToObject(item, "kind", kind);

      const char *typeStr = "Unknown";
      if (lspSymbols[i].type == LSP_TYPE_NUMBER)
        typeStr = "Number";
      else if (lspSymbols[i].type == LSP_TYPE_STRING)
        typeStr = "String";
      else if (lspSymbols[i].type == LSP_TYPE_LIST)
        typeStr = "List";
      else if (lspSymbols[i].type == LSP_TYPE_DICTIONARY)
        typeStr = "Dict";
      else if (lspSymbols[i].type == LSP_TYPE_BLUEPRINT)
        typeStr = "Blueprint";

      char desc[128];
      if (lspSymbols[i].blueprintName[0] != '\0') {
        snprintf(desc, sizeof(desc), "MOON %s (Instance of %s)", typeStr,
                 lspSymbols[i].blueprintName);
      } else {
        snprintf(desc, sizeof(desc), "MOON %s (Depth %d)", typeStr,
                 lspSymbols[i].depth);
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

            Node *ast = parseSource(textVal->valuestring);
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
      }
      cJSON_Delete(request);
    }

    free(payload);
  }
}
