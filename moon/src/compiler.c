#include "compiler.h"
#include "ast.h"
#include "codegen.h"
#include "debug.h" // <--- 1. ADDED THIS
#include "parser.h"

// The Master Compilation Pipeline
ObjFunction *compile(const char *source) {
  // NEW: Phase 0 - Hoisting
  hoistPhrases(source);

  // Phase 1: Front-End (Source Code -> Abstract Syntax Tree)
  Node *ast = parseSource(source);

  // If the parser found syntax errors, we abort before generating bad bytecode.
  if (ast == NULL || parser.hadError) {
    if (ast != NULL)
      freeNode(ast);
    return NULL;
  }

  // --- 2. THE AST INTERCEPT ---
  // Catch the tree before it gets compiled into bytecode!
  if (printAstFlag) {
    printf("=== ABSTRACT SYNTAX TREE ===\n");
    printAST(ast, 0);
    printf("============================\n");

    freeNode(ast);
    return NULL; // Halt compilation so the VM doesn't try to run empty code
  }
  // ----------------------------

  // Phase 2: Back-End (Abstract Syntax Tree -> VM Bytecode)
  ObjFunction *function = generateCode(ast);

  // Phase 3: Cleanup
  freeNode(ast);

  return function;
}
