#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "error.h"
#include "lsp.h"
#include "vm.h"

// --- GLOBAL FLAGS ---
bool printAstFlag = false;
bool isReplMode = false;

// --- THE SMART PROMPT CALCULATOR ---
static int calculateBlockDepth(const char *source) {
  initScanner(source);
  int depth = 0;
  Token token = scanToken();

  while (token.type != TOKEN_EOF) {
    // Openers
    if (token.type == TOKEN_LEFT_BRACE || token.type == TOKEN_LEFT_BRACKET) {
      depth++;
    }

    // Closers
    else if (token.type == TOKEN_RIGHT_BRACE ||
             token.type == TOKEN_RIGHT_BRACKET || token.type == TOKEN_END) {
      depth--;
    }

    // The Colon Rule
    else if (token.type == TOKEN_COLON) {
      // In MOON, if a colon is the last meaningful character on a line, it
      // opens a block!
      const char *p = scanner.current;
      while (*p == ' ' || *p == '\t' || *p == '\r')
        p++;                                       // Skip whitespace
      if (*p == '\n' || *p == '\0' || *p == '#') { // Hit a newline or comment!
        depth++;
      }
    }
    token = scanToken();
  }

  // Never return negative depth, even if they type an orphan 'end'
  return depth < 0 ? 0 : depth;
}

static void repl() {
  printf(COLOR_CYAN
         "================================================\n" COLOR_RESET);
  printf(COLOR_CYAN " 🌙 M.O.O.N. Interactive Terminal v1.0\n" COLOR_RESET);
  printf(COLOR_DIM "    Type 'quit' or 'exit' to shut down.\n" COLOR_RESET);
  printf(COLOR_CYAN
         "================================================\n\n" COLOR_RESET);

  isReplMode = true;

  char source[8192] = ""; // The Multi-Line Accumulator Buffer
  char line[1024];

  for (;;) {
    int currentDepth = calculateBlockDepth(source);

    // 1. The Dynamic Prompt!
    if (currentDepth == 0) {
      printf(COLOR_CYAN " moon" COLOR_DIM " > " COLOR_RESET);
    } else {
      // The CLI Tree Engine!
      printf(COLOR_DIM "      ");

      // Print vertical pipes for previous levels of depth
      for (int i = 0; i < currentDepth - 1; i++) {
        printf("│   ");
      }

      // Print the branch connector for the current line
      printf("├─> " COLOR_RESET);
    }

    // 2. Grab the input
    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    // 3. Clean Exits (Only allow quit if we aren't in the middle of a block)
    if (currentDepth == 0 &&
        (strncmp(line, "quit", 4) == 0 || strncmp(line, "exit", 4) == 0)) {
      printf(COLOR_DIM "Shutting down orbital terminal...\n" COLOR_RESET);
      break;
    }

    // 4. Append the new line to our accumulator
    if (strlen(source) + strlen(line) < sizeof(source)) {
      strcat(source, line);
    } else {
      printf(COLOR_RED "Input buffer overflow! Resetting..." COLOR_RESET "\n");
      source[0] = '\0';
      continue;
    }

    // 5. Check if the block is finally closed!
    if (calculateBlockDepth(source) == 0) {

      // Empty Line Shield (Don't compile pure whitespace)
      bool isEmpty = true;
      for (int i = 0; source[i] != '\0'; i++) {
        if (source[i] != ' ' && source[i] != '\t' && source[i] != '\n' &&
            source[i] != '\r') {
          isEmpty = false;
          break;
        }
      }

      if (!isEmpty) {
        interpret(source); // Execute the whole chunk at once!
      }

      // Reset the accumulator for the next command
      source[0] = '\0';
    }
  }
}

char *readFile(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char *buffer = malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }

  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

static void runFile(const char *path) {
  char *source = readFile(path);
  InterpretResult result = interpret(source);
  free(source);

  if (result == INTERPRET_COMPILE_ERROR)
    exit(65);
  if (result == INTERPRET_RUNTIME_ERROR)
    exit(70);
}

int main(int argc, char *argv[]) {
  initVM();

  char *filePath = NULL;

  bool runLsp = false;

  // --- THE ROBUST CLI PARSER ---
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--debug") == 0) {
      vm.debugMode = true;
    } else if (strcmp(argv[i], "--ast") == 0) {
      printAstFlag = true;
    } else if (strcmp(argv[i], "--lsp") == 0) {
      runLsp = true;
      isLspMode = true; // Tell the Error Engine to stay quiet!
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "Unknown flag: %s\n", argv[i]);
      fprintf(stderr, "Usage: moon [--debug] [--ast] [--lsp] [path]\n");
      exit(64);
    } else {
      if (filePath != NULL) {
        fprintf(stderr, "Error: Multiple file paths provided.\n");
        exit(64);
      }
      filePath = argv[i];
    }
  }

  // --- EXECUTION ROUTING ---
  // 2. Add the LSP branch
  if (runLsp) {
    startLanguageServer();
  } else if (filePath == NULL) {
    repl();
  } else {
    runFile(filePath);
  }

  freeVM();
  return 0;
}
