#include "compiler.h"
#include "ast.h"
#include "codegen.h"
#include "debug.h" // <--- 1. ADDED THIS
#include "emitter.h"
#include "parser.h"

// The Master Compilation Pipeline
ObjFunction *compile(const char *source, ObjModule *module, int startLine) {
  currentModule = module;
  // REMOVED: currentGlobals = &module->fields;  <--- Let vm.c handle this!

  // Phase 1: Front-End (Source Code -> Abstract Syntax Tree)
  Node *ast = parseSource(source, startLine);

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

  // --- ADD THIS SAFETY CATCH ---
  // If emitting bytecode threw an error (like "too many constants"), abort!
  // Note: We do NOT need to manually free `function` or its bytecode Chunk here. 
  // It is tracked by the VM as an ObjFunction, and because we are returning NULL 
  // without storing it in a GC Root, the Garbage Collector will automatically sweep 
  // it away on the next cycle!
  if (parser.hadError)
    return NULL;

  return function;
}
