#ifndef moon_codegen_h
#define moon_codegen_h

#include "ast.h"
#include "object.h"

// The Master Entry Point for the Back-End.
// Takes the root of our AST, walks it, and returns the compiled
// bytecode function ready for the Virtual Machine to execute.
ObjFunction *generateCode(Node *rootAST);

#endif
