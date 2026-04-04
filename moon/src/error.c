#include "error.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- THE SPELLCHECKING ORACLE ---
// Calculates the exact number of edits required to change s1 into s2.
static int levenshteinDistance(const char *s1, const char *s2) {
  int len1 = strlen(s1);
  int len2 = strlen(s2);

  // We use a 1D array to save memory (only keeping the previous row)
  int *column = malloc((len1 + 1) * sizeof(int));
  for (int i = 0; i <= len1; i++) {
    column[i] = i;
  }

  for (int x = 1; x <= len2; x++) {
    column[0] = x;
    int lastDiagonal = x - 1;
    for (int y = 1; y <= len1; y++) {
      int oldDiagonal = column[y];
      int cost = (s1[y - 1] == s2[x - 1]) ? 0 : 1;

      int min = column[y] + 1; // Deletion
      if (column[y - 1] + 1 < min)
        min = column[y - 1] + 1; // Insertion
      if (lastDiagonal + cost < min)
        min = lastDiagonal + cost; // Substitution

      column[y - 1] = lastDiagonal;
      lastDiagonal = oldDiagonal;
      column[y] = min;
    }
  }

  int result = column[len1];
  free(column);
  return result;
}

// Scans the global environment to find the closest matching variable name.
const char *findVariableSuggestion(const char *misspelled) {
  const char *bestMatch = NULL;
  int minDistance = 999;

  // What is a "reasonable" typo?
  // For a 3-letter word, 1 edit is okay. For a 10-letter word, 3 edits is okay.
  int maxAllowedDistance = (strlen(misspelled) <= 3) ? 1 : 2;

  // Scan every variable currently registered in the VM!
  for (int i = 0; i < vm.globals.capacity; i++) {
    Entry *entry = &vm.globals.entries[i];

    // Skip empty slots and tombstones
    if (IS_EMPTY(entry->key) || IS_TOMB(entry->key))
      continue;

    // Global keys are ALWAYS strings in MOON
    if (IS_STRING(entry->key)) {
      const char *globalName = AS_CSTRING(entry->key);

      int distance = levenshteinDistance(misspelled, globalName);

      if (distance < minDistance && distance <= maxAllowedDistance) {
        minDistance = distance;
        bestMatch = globalName;
      }
    }
  }

  return bestMatch;
}

// The global anchor for the raw source code string
static const char *globalSource = NULL;

void initErrorEngine(const char *source) { globalSource = source; }

static const char *getErrorTypeName(ErrorType type) {
  switch (type) {
  case ERR_SYNTAX:
    return "Syntax Error";
  case ERR_TYPE:
    return "Type Error";
  case ERR_REFERENCE:
    return "Reference Error";
  case ERR_RUNTIME:
    return "Runtime Error";
  default:
    return "Error";
  }
}

// The heart of the engine: Locates and prints the exact line of code
static void printSnippet(int targetLine, int targetCol, int length) {
  if (globalSource == NULL)
    return;

  // 1. Scan forward to find the start of the target line
  const char *lineStart = globalSource;
  int currentLine = 1;

  while (currentLine < targetLine && *lineStart != '\0') {
    if (*lineStart == '\n') {
      currentLine++;
    }
    lineStart++;
  }

  // 2. Scan forward to find the end of this specific line
  const char *lineEnd = lineStart;
  while (*lineEnd != '\n' && *lineEnd != '\0') {
    lineEnd++;
  }

  int lineLen = (int)(lineEnd - lineStart);

  // 3. Print the code snippet! (Dim line number, Cyan code)
  printf("\n" COLOR_DIM "  %d | " COLOR_CYAN "%.*s\n" COLOR_RESET, targetLine,
         lineLen, lineStart);

  // 4. Print the squiggles! (Dim pipe, Red squiggles)
  if (targetCol > 0) {
    // Compile Error: We have exact token coordinates
    printf(COLOR_DIM "    | " COLOR_RED);
    for (int i = 1; i < targetCol; i++) {
      printf(" ");
    }
    int drawLength = length > 0 ? length : 1;
    for (int i = 0; i < drawLength; i++) {
      printf("^");
    }
    printf(COLOR_RESET "\n");
  } else {
    // Runtime Error: We highlight the whole line, skipping leading spaces!
    printf(COLOR_DIM "    | " COLOR_RED);
    int startOffset = 0;
    while (startOffset < lineLen &&
           (lineStart[startOffset] == ' ' || lineStart[startOffset] == '\t')) {
      printf(" ");
      startOffset++;
    }
    for (int i = startOffset; i < lineLen; i++) {
      printf("^");
    }
    printf(COLOR_RESET "\n");
  }
  printf("\n");
}

void reportCompileError(Token *token, ErrorType type, const char *message,
                        const char *hint) {
  fprintf(stderr, "\n" COLOR_RED "Oops! %s on line %d\n" COLOR_RESET,
          getErrorTypeName(type), token->line);
  fprintf(stderr,
          "I found a problem near '" COLOR_YELLOW "%.*s" COLOR_RESET "':\n",
          token->length, token->start);

  printSnippet(token->line, token->column, token->length);

  fprintf(stderr, COLOR_RED "Reason: " COLOR_RESET "%s\n", message);

  if (hint != NULL) {
    fprintf(stderr, "\n" COLOR_YELLOW "Hint: " COLOR_RESET "%s\n\n", hint);
  }
}

// Add #include "vm.h" to the top of error.c so it can access the Vault!

void reportRuntimeError(ObjString *moduleName, int line, ErrorType type,
                        const char *message, const char *hint) {
  const char *modNameC = moduleName != NULL ? moduleName->chars : "<unknown>";

  fprintf(stderr, "\n" COLOR_RED "Oops! %s in '%s' on line %d\n" COLOR_RESET,
          getErrorTypeName(type), modNameC, line);
  fprintf(stderr, "The program crashed while trying to execute this:\n");

  // --- THE VAULT LOOKUP ---
  Value sourceVal;
  if (moduleName != NULL &&
      tableGet(&vm.loadedModules, OBJ_VAL(moduleName), &sourceVal)) {
    const char *oldSource = globalSource;
    globalSource = AS_CSTRING(sourceVal);

    printSnippet(line, 0, 0); // Draw the squiggles!

    globalSource = oldSource;
  } else {
    fprintf(stderr, COLOR_DIM "  (Source code unavailable)\n\n" COLOR_RESET);
  }

  fprintf(stderr, COLOR_RED "Reason: " COLOR_RESET "%s\n", message);

  if (hint != NULL) {
    fprintf(stderr, "\n" COLOR_YELLOW "Hint: " COLOR_RESET "%s\n\n", hint);
  }
}
