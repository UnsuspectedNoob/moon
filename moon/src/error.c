#include "error.h"
#include <stdio.h>
#include <string.h>

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

  // 3. Print the code snippet!
  printf("\n  %d | %.*s\n", targetLine, lineLen, lineStart);

  // 4. Print the squiggles!
  if (targetCol > 0) {
    // Compile Error: We have exact token coordinates
    printf("    | ");
    for (int i = 1; i < targetCol; i++) {
      printf(" ");
    }
    int drawLength = length > 0 ? length : 1;
    for (int i = 0; i < drawLength; i++) {
      printf("^");
    }
    printf("\n");
  } else {
    // Runtime Error: We highlight the whole line, skipping leading spaces!
    printf("    | ");
    int startOffset = 0;
    // Skip leading whitespace for a cleaner look
    while (startOffset < lineLen &&
           (lineStart[startOffset] == ' ' || lineStart[startOffset] == '\t')) {
      printf(" ");
      startOffset++;
    }
    // Draw squiggles for the rest of the line
    for (int i = startOffset; i < lineLen; i++) {
      printf("^");
    }
    printf("\n");
  }
  printf("\n");
}

void reportCompileError(Token *token, ErrorType type, const char *message,
                        const char *hint) {
  fprintf(stderr, "\nOops! %s on line %d\n", getErrorTypeName(type),
          token->line);
  fprintf(stderr, "I found a problem near '%.*s':\n", token->length,
          token->start);

  printSnippet(token->line, token->column, token->length);

  // WE ADDED THIS LINE!
  fprintf(stderr, "Reason: %s\n", message);

  if (hint != NULL) {
    fprintf(stderr, "\nHint: %s\n\n", hint);
  }
}

void reportRuntimeError(int line, ErrorType type, const char *message,
                        const char *hint) {
  fprintf(stderr, "\nOops! %s on line %d\n", getErrorTypeName(type), line);
  fprintf(stderr, "The program crashed while trying to execute this:\n");

  // For runtime errors, we might not have the exact column from the bytecode
  // chunk yet, so we pass 0 for the column to just highlight the line.
  printSnippet(line, 0, 0);

  fprintf(stderr, "Reason: %s\n", message);

  if (hint != NULL) {
    fprintf(stderr, "\nHint: %s\n\n", hint);
  }
}
