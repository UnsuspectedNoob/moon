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

static void repl() {
  char line[2048];
  for (;;) {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    interpret(line);
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
