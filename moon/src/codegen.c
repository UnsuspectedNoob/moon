#include "codegen.h"
#include "ast.h"
#include <stdint.h>
#include <stdlib.h>

#include "chunk.h"
#include "emitter.h"
#include "parser.h"
#include "scanner.h"

extern bool isReplMode;
// ==========================================
// CODEGEN STATE (Scope & Locals)
// ==========================================

static uint16_t identifierConstant(Token *name) { // <--- Change to uint16_t
  return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

// ==========================================
// LOOP STATE TRACKER
// ==========================================

typedef struct sCompilerLoop {
  int start;      // Byte offset of the loop's start (for 'skip')
  int scopeDepth; // Scope depth at loop entry (for cleanup)

  int breakJumps[256]; // Array of jump offsets waiting to be patched
  int breakCount;

  struct sCompilerLoop *enclosing; // Point to the outer loop if nested
} CompilerLoop;

static CompilerLoop *currentLoop = NULL; // The active loop

static void startLoop(CompilerLoop *loop, int startByte) {
  loop->enclosing = currentLoop;          // Link to the outer loop (if nested)
  loop->start = startByte;                // Where 'skip' jumps to
  loop->scopeDepth = current->scopeDepth; // The depth at loop entry!
  loop->breakCount = 0;

  currentLoop = loop; // Make this the active loop
}

static void endLoop() {
  // Backpatch every single 'break' jump that was recorded in this loop!
  for (int i = 0; i < currentLoop->breakCount; i++) {
    patchJump(currentLoop->breakJumps[i]);
  }

  // Restore the previous enclosing loop
  currentLoop = currentLoop->enclosing;
}

// Forward declaration for the recursive walker
static void walkNode(Node *node);

// ==========================================
// THE TREE WALKER
// ==========================================

static void walkNode(Node *node) {
  if (node == NULL)
    return;

  // --- THE SYNCHRONIZATION FIX ---
  // Lock the emitter's line number to the current AST node's line number!
  if (node->line > 0) {
    currentLine = node->line;
  }
  // -------------------------------

  switch (node->type) {

    // --- 1. LITERALS & MATH (Post-Order Traversal) ---

  case NODE_LITERAL: {
    // Leaf node: Just push the value onto the VM stack
    emitConstant(node->as.literal.value);
    break;
  }

  case NODE_UNARY: {
    walkNode(node->as.unary.right); // Walk the child first

    if (node->as.unary.opToken.type == TOKEN_MINUS)
      emitByte(OP_NEGATE);
    else if (node->as.unary.opToken.type == TOKEN_NOT)
      emitByte(OP_NOT);
    break;
  }

  case NODE_BINARY: {
    // 1. Walk the left branch (pushes left value to stack)
    walkNode(node->as.binary.left);
    current->temporaries++;
    // 2. Walk the right branch (pushes right value to stack)
    walkNode(node->as.binary.right);
    current->temporaries--;

    // 3. Emit the operator instruction!
    // The VM will pop the two values, apply this operator, and push the result.
    switch (node->as.binary.opToken.type) {
    case TOKEN_PLUS:
      emitByte(OP_ADD);
      break;
    case TOKEN_ADD_INPLACE:
      emitByte(OP_ADD_INPLACE);
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
    case TOKEN_MOD:
      emitByte(OP_MOD);
      break;
    case TOKEN_EQUAL_EQUAL:
    case TOKEN_EQUAL:
      emitByte(OP_EQUAL);
      break;
    case TOKEN_IS:
      emitByte(OP_EQUAL);
      break;
    case TOKEN_GREATER:
      emitByte(OP_GREATER);
      break;
    case TOKEN_LESS:
      emitByte(OP_LESS);
      break;
    case TOKEN_GREATER_EQUAL:
      emitByte(OP_LESS);
      emitByte(OP_NOT);
      break;
    case TOKEN_LESS_EQUAL:
      emitByte(OP_GREATER);
      emitByte(OP_NOT);
      break;
    default:
      break;
    }
    break;
  }

    // --- 2. ACTIONS ---

  case NODE_EXPRESSION_STMT: {
    walkNode(node->as.singleExpr.expression);

    // --- THE CALCULATOR FIX ---
    // If we are in the REPL, and we are at the top level (not inside a block or
    // loop), print the result instead of throwing it in the garbage!
    if (isReplMode && current->scopeDepth == 0) {
      emitByte(OP_SHOW_REPL);
    } else {
      emitByte(OP_POP); // Standard script behavior
    }
    // --------------------------
    break;
  }

  case NODE_BLOCK: {
    // If it has no parent, it's the root script! Don't bump the scope!
    bool isScript = (node->parent == NULL);

    if (!isScript)
      beginScope();

    for (int i = 0; i < node->as.block.count; i++) {
      walkNode(node->as.block.statements[i]);
    }

    if (!isScript)
      endScope();
    break;
  }

    // --- 3. CONTROL FLOW (The Snip-Fix!) ---

  case NODE_IF: {
    // 1. Walk the condition (leaves a true/false on the stack)
    walkNode(node->as.ifStmt.condition);

    // 2. Emit the jump to skip the 'then' block if false.
    // We save the offset because we don't know how big the 'then' block is yet!
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // Clean up the condition bool

    // 3. Walk the 'then' block
    if (node->as.ifStmt.thenBranch != NULL) {
      walkNode(node->as.ifStmt.thenBranch);
    }

    // 4. Emit a jump to skip over the 'else' block
    int elseJump = emitJump(OP_JUMP);

    // 5. Patch the FIRST jump so it lands exactly here
    patchJump(thenJump);
    emitByte(OP_POP);

    // 6. Walk the 'else' block
    if (node->as.ifStmt.elseBranch != NULL) {
      walkNode(node->as.ifStmt.elseBranch);
    }

    // 7. Patch the SECOND jump so the 'then' block lands exactly here
    patchJump(elseJump);
    break;
  }

  case NODE_LOGICAL: {
    walkNode(node->as.binary.left);

    if (node->as.binary.opToken.type == TOKEN_OR) {
      // 'OR' Logic:
      // If left is false, jump to evaluate the right side.
      int elseJump = emitJump(OP_JUMP_IF_FALSE);

      // If left is true, we fall through to here and unconditionally jump to
      // the end!
      int endJump = emitJump(OP_JUMP);

      patchJump(elseJump);
      emitByte(OP_POP); // Clean up the left value (it was false)

      walkNode(node->as.binary.right);

      patchJump(endJump);
    } else {
      // 'AND' Logic:
      // If left is false, short-circuit and jump to the end.
      int endJump = emitJump(OP_JUMP_IF_FALSE);

      emitByte(OP_POP); // Clean up the left value (it was true)
      walkNode(node->as.binary.right);

      patchJump(endJump);
    }
    break;
  }

  case NODE_WHILE: {
    int loopStart = currentChunk()->count; // mark where to jump back to

    // --- TRACKER START ---
    CompilerLoop loop;
    startLoop(&loop, loopStart);

    walkNode(node->as.whileStmt.condition);

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // clean up condition

    walkNode(node->as.whileStmt.body);

    emitLoop(loopStart); // Jump back to condition
    patchJump(exitJump); // Where to land when the loop breaks
    emitByte(OP_POP);

    // --- TRACKER END ---
    endLoop();
    break;
  }

  case NODE_FOR: {
    // --- SCOPE 1: The Outer Loop Scope ---
    beginScope();

    // 1. Evaluate the Sequence (pushes it to the physical stack)
    walkNode(node->as.forStmt.sequence);

    // RESERVATION 1: Tell the compiler a sequence is sitting on the stack!
    // We start the name with a space so the user can never type it.
    Token seqToken = {.type = TOKEN_IDENTIFIER, .start = " seq", .length = 4};
    addLocal(seqToken);
    markInitialized();

    // 2. Setup the Hidden Iterator (pushes -1 to the stack)
    emitByte(OP_GET_ITER);

    // RESERVATION 2: Tell the compiler the index is sitting on the stack!
    Token iterToken = {.type = TOKEN_IDENTIFIER, .start = " iter", .length = 5};
    addLocal(iterToken);
    markInitialized();

    // 3. Mark the top of the loop
    int loopStart = currentChunk()->count;

    // --- TRACKER START ---
    // We start tracking HERE, so 'skip' jumps to OP_FOR_ITER
    CompilerLoop loop;
    startLoop(&loop, loopStart);

    // 4. The Iteration Check
    // Replace the old iterSlot and jump logic with this in BOTH places:
    Token iterTok = syntheticToken(" iter");
    int iterSlot = resolveLocal(current, &iterTok);

    emitByte(OP_FOR_ITER);
    emitByte((uint8_t)iterSlot);
    emitByte(0xff); // Placeholder for High Jump Byte
    emitByte(0xff); // Placeholder for Low Jump Byte
    int exitJump = currentChunk()->count - 2;

    // --- SCOPE 2: The Inner Body Scope ---
    beginScope();

    // 5. Register the Loop Variable
    addLocal(node->as.forStmt.iterator);
    markInitialized();

    // --- NEW: SECOND VARIABLE UNPACKING ---
    if (node->as.forStmt.hasIndex) {
      Token iterTok = syntheticToken(" iter");
      int iterSlot = resolveLocal(current, &iterTok);

      // Emit the magic extraction opcode!
      emitBytes(OP_GET_ITER_VALUE, (uint8_t)iterSlot);

      addLocal(node->as.forStmt.indexVar);
      markInitialized();
    }
    // --------------------------------------

    // 6. Execute the body
    walkNode(node->as.forStmt.body);

    // 7. Close Inner Scope (Automatically emits OP_POP for 'item')
    endScope();

    // 8. Loop back up
    emitLoop(loopStart);

    // 9. The Exit Point
    patchJump(exitJump);

    // --- TRACKER END ---
    endLoop();

    // 10. Close Outer Scope
    // Automatically emits OP_POP twice to clean up " seq" and " iter"!
    endScope();
    break;
  }

  case NODE_BREAK: {
    if (currentLoop == NULL)
      return; // (Sanity check)

    // 1. STACK CLEANUP: Pop any locals created inside the loop body
    for (int i = current->localCount - 1;
         i >= 0 && current->locals[i].depth > currentLoop->scopeDepth; i--) {
      emitByte(OP_POP);
    }

    // 2. Emit the dummy jump and record its location in the tracker!
    int jump = emitJump(OP_JUMP);
    currentLoop->breakJumps[currentLoop->breakCount++] = jump;
    break;
  }

  case NODE_SKIP: {
    if (currentLoop == NULL)
      return;

    // 1. STACK CLEANUP: Pop locals (exact same logic as break)
    for (int i = current->localCount - 1;
         i >= 0 && current->locals[i].depth > currentLoop->scopeDepth; i--) {
      emitByte(OP_POP);
    }

    // 2. Jump backwards to the top of the loop
    emitLoop(currentLoop->start);
    break;
  }

    // --- 4. VARIABLES & SCOPE ---

  case NODE_VARIABLE: {
    // 1. Is it a local variable? (Inside a block/function)
    int arg = resolveLocal(current, &node->as.variable.name);

    if (arg != -1) {
      // Yes: It lives on the VM stack at a specific offset
      emitBytes(OP_GET_LOCAL, (uint8_t)arg);
    } else {
      uint16_t globalName = identifierConstant(&node->as.variable.name);

      emitByte(OP_GET_GLOBAL);
      emitByte((globalName >> 8) & 0xff); // High byte
      emitByte(globalName & 0xff);        // Low byte
    }
    break;
  }

  case NODE_LET: {
    for (int i = 0; i < node->as.let.nameCount; i++) {

      // RULE 2 & 3: Decide which expression to evaluate
      if (node->as.let.exprCount == 1) {
        // N-to-1 Broadcast: Always evaluate the 0th expression
        walkNode(node->as.let.exprs[0]);
      } else {
        // N-to-N: Evaluate the specifically matching expression
        walkNode(node->as.let.exprs[i]);
      }

      // Memory Storage Logic
      if (current->scopeDepth > 0) {
        addLocal(node->as.let.names[i]);
        markInitialized();
      } else {
        uint16_t globalName = identifierConstant(&node->as.let.names[i]);
        emitByte(OP_DEFINE_GLOBAL);
        emitByte((globalName >> 8) & 0xff);
        emitByte(globalName & 0xff);
        emitByte(OP_POP);
      }
    }
    break;
  }

  case NODE_LIST: {
    for (int i = 0; i < node->as.list.count; i++) {
      walkNode(node->as.list.items[i]);
      current->temporaries++; // <-- TRACK EACH ITEM
    }

    emitByte(OP_BUILD_LIST);
    uint16_t count = (uint16_t)node->as.list.count;
    emitByte((count >> 8) & 0xff); // High byte
    emitByte(count & 0xff);        // Low byte

    // UNTRACK ALL ITEMS
    current->temporaries -= node->as.list.count;
    break;
  }

  case NODE_DICT: {
    for (int i = 0; i < node->as.dictExpr.count; i++) {
      walkNode(node->as.dictExpr.keys[i]);
      current->temporaries++; // <-- TRACK KEY
      walkNode(node->as.dictExpr.values[i]);
      current->temporaries++; // <-- TRACK VALUE
    }

    emitByte(OP_BUILD_DICT);
    uint16_t count = (uint16_t)node->as.dictExpr.count;
    emitByte((count >> 8) & 0xff); // High byte
    emitByte(count & 0xff);        // Low byte

    // UNTRACK ALL KEYS AND VALUES
    current->temporaries -= (node->as.dictExpr.count * 2);
    break;
  }

  case NODE_RANGE: {
    // Evaluate the 3 parts of the range in order: start, end, step
    walkNode(node->as.range.start);
    current->temporaries++; // <-- TRACK START
    walkNode(node->as.range.end);
    current->temporaries++; // <-- TRACK START
    walkNode(node->as.range.step);
    current->temporaries -= 2; // <-- UNTRACK START AND END BEFORE OP

    // VM pops all 3 and pushes the Range object
    emitByte(OP_RANGE);
    break;
  }

  case NODE_SUBSCRIPT: {
    walkNode(node->as.subscript.left);

    // 1. Tell the VM to remember this collection!
    emitByte(OP_PUSH_SEQUENCE);

    current->temporaries++; // <-- TRACK SEQUENCE
    walkNode(node->as.subscript.index);
    current->temporaries--; // <-- UNTRACK SEQUENCE BEFORE OP

    emitByte(OP_GET_SUBSCRIPT);

    // 2. We are done with the brackets, forget the collection!
    emitByte(OP_POP_SEQUENCE);
    break;
  }

  case NODE_END: {
    emitByte(OP_GET_END_INDEX); // Back to a pure 1-byte opcode!
    break;
  }

  case NODE_PROPERTY: {
    // 1. Push the target object to the stack
    walkNode(node->as.property.target);

    // 2. Convert the literal token into a 16-bit Constant Index
    uint16_t nameConst = identifierConstant(&node->as.property.name);

    // 3. Emit the instruction + the 16-bit index!
    emitByte(OP_GET_PROPERTY);
    emitByte((nameConst >> 8) & 0xff);
    emitByte(nameConst & 0xff);
    break;
  }

    // --- 5. ASSIGNMENT ---

  case NODE_SET: {
    // --- PHASE 1: Evaluate LHS Addresses ---
    for (int i = 0; i < node->as.set.targetCount; i++) {
      Node *target = node->as.set.targets[i];
      if (target->type == NODE_SUBSCRIPT) {
        walkNode(target->as.subscript.left);
        emitByte(OP_PUSH_SEQUENCE); // <--- TRACK IT!
        walkNode(target->as.subscript.index);
      } else if (target->type == NODE_PROPERTY) {
        walkNode(target->as.property.target);
      }
    }

    // --- PHASE 2: Evaluate RHS Values (Left-to-Right) ---
    for (int i = 0; i < node->as.set.targetCount; i++) {
      Node *expr = (node->as.set.valueCount == 1) ? node->as.set.values[0]
                                                  : node->as.set.values[i];
      walkNode(expr);
      current->temporaries++; // <-- TRACK EACH EVALUATED VALUE ON THE STACK
    }

    current->temporaries -= node->as.set.targetCount;

    // --- PHASE 3: Assign and Pop (Right-to-Left) ---
    // Because values are piled up on the stack, the last value is at the top!
    for (int i = node->as.set.targetCount - 1; i >= 0; i--) {
      Node *target = node->as.set.targets[i];

      if (target->type == NODE_VARIABLE) {
        int localArg = resolveLocal(current, &target->as.variable.name);
        if (localArg != -1) {
          emitBytes(OP_SET_LOCAL, (uint8_t)localArg);
        } else {
          uint16_t globalName = identifierConstant(&target->as.variable.name);
          emitByte(OP_SET_GLOBAL);
          emitByte((globalName >> 8) & 0xff);
          emitByte(globalName & 0xff);
        }
        emitByte(OP_POP);
      } else if (target->type == NODE_SUBSCRIPT) {
        emitByte(OP_SET_SUBSCRIPT);
        emitByte(OP_POP);
        emitByte(OP_POP_SEQUENCE); // <--- CLEAN IT UP!
      } else if (target->type == NODE_PROPERTY) {
        uint16_t nameConst = identifierConstant(&target->as.property.name);
        emitByte(OP_SET_PROPERTY);
        emitByte((nameConst >> 8) & 0xff);
        emitByte(nameConst & 0xff);
        emitByte(OP_POP);
      }
    }

    break;
  }

  case NODE_INTERPOLATION: {
    for (int i = 0; i < node->as.interpolation.partCount; i++) {
      walkNode(node->as.interpolation.parts[i]);
    }
    emitByte(OP_BUILD_STRING);
    uint16_t count = (uint16_t)node->as.interpolation.partCount;
    emitByte((count >> 8) & 0xff); // High byte
    emitByte(count & 0xff);        // Low byte
    break;
  }

  case NODE_CALL: {
    walkNode(node->as.call.callee);
    current->temporaries++; // <-- TRACK CALLEE

    for (int i = 0; i < node->as.call.argCount; i++) {
      walkNode(node->as.call.arguments[i]);
      current->temporaries++; // <-- TRACK EACH ARGUMENT
    }

    emitByte(OP_CALL);
    uint16_t count = (uint16_t)node->as.call.argCount;
    emitByte((count >> 8) & 0xff); // High byte
    emitByte(count & 0xff);        // Low byte

    // UNTRACK CALLEE + ALL ARGUMENTS
    current->temporaries -= (1 + node->as.call.argCount);
    break;
  }

  case NODE_PHRASAL_CALL: {
    int arg = resolveLocal(current, &node->as.phrasalCall.mangledName);
    if (arg != -1) {
      emitBytes(OP_GET_LOCAL, (uint8_t)arg);
    } else {
      uint16_t nameConstant =
          identifierConstant(&node->as.phrasalCall.mangledName);
      emitByte(OP_GET_GLOBAL);
      emitByte((nameConstant >> 8) & 0xff);
      emitByte(nameConstant & 0xff);
    }

    current->temporaries++; // <-- TRACK CALLEE

    for (int i = 0; i < node->as.phrasalCall.argCount; i++) {
      walkNode(node->as.phrasalCall.arguments[i]);
      current->temporaries++; // <-- TRACK EACH ARGUMENT
    }

    emitByte(OP_CALL);
    uint16_t count = (uint16_t)node->as.phrasalCall.argCount;
    emitByte((count >> 8) & 0xff); // High byte
    emitByte(count & 0xff);        // Low byte

    // UNTRACK CALLEE + ALL ARGUMENTS
    current->temporaries -= (1 + node->as.phrasalCall.argCount);
    break;
  }

  case NODE_CAST: {
    walkNode(node->as.cast.left);
    current->temporaries++; // <-- TRACK LEFT
    walkNode(node->as.cast.right);
    current->temporaries--; // <-- UNTRACK LEFT BEFORE OP
    emitByte(OP_CAST);
    break;
  }

  case NODE_RETURN: {
    if (node->as.singleExpr.expression != NULL) {
      walkNode(node->as.singleExpr.expression);
    } else {
      emitByte(OP_NIL);
    }
    emitByte(OP_RETURN);
    break;
  }

  case NODE_FUNCTION: {
    // 1. Boot up a new compiler context for this function
    Compiler fnCompiler;
    initCompiler(&fnCompiler, TYPE_FUNCTION);

    fnCompiler.function->name =
        copyString(node->as.function.name.start, node->as.function.name.length);

    // 2. Open scope and inject the parameters as local variables!
    beginScope();
    for (int i = 0; i < node->as.function.paramCount; i++) {
      fnCompiler.function->arity++;
      addLocal(node->as.function.parameters[i]);
      markInitialized();
    }

    // 3. Walk the body of the function
    walkNode(node->as.function.body);

    // 4. Close the compiler and get the finished object
    ObjFunction *fn = endCompiler();

    // 1. Push the Expected Types to the Stack!
    // We walk the AST node! If it's a simple variable (String), it pushes the
    // Blueprint. If it's a Union (String or Number), it evaluates both and
    // pushes an ObjUnion!
    for (int i = 0; i < node->as.function.paramCount; i++) {
      walkNode(node->as.function.paramTypes[i]);
    }

    // 2. Push the compiled function chunk itself
    uint8_t fnConstant = makeConstant(OBJ_VAL(fn));
    emitBytes(OP_CONSTANT, fnConstant);

    // 3. Emit the Injector Opcode!
    uint16_t globalName = identifierConstant(&node->as.function.name);
    emitByte(OP_DEFINE_METHOD);
    emitByte((globalName >> 8) & 0xff);
    emitByte(globalName & 0xff);

    // (Note: We skip the local scope logic for methods. Multiple Dispatch
    // methods must be globally accessible).
    break;
  }

  case NODE_TYPE_DECL: {
    for (int i = 0; i < node->as.typeDecl.count; i++) {
      // 1. Push the property name as a String Constant
      Value nameVal =
          OBJ_VAL(copyString(node->as.typeDecl.propertyNames[i].start,
                             node->as.typeDecl.propertyNames[i].length));
      emitConstant(nameVal);
      // 2. Push the default value expression
      walkNode(node->as.typeDecl.defaultValues[i]);
    }

    // 3. Emit the opcode, the Type's Name, and the Property Count
    uint16_t typeName = identifierConstant(&node->as.typeDecl.name);
    emitByte(OP_TYPE_DEF);
    emitByte((typeName >> 8) & 0xff);
    emitByte(typeName & 0xff);

    uint16_t count = (uint16_t)node->as.typeDecl.count;
    emitByte((count >> 8) & 0xff);
    emitByte(count & 0xff);
    break;
  }

  case NODE_LOAD: {
    // 1. Strip the quotes: '"math.moon"' becomes 'math.moon'
    ObjString *pathStr = copyString(node->as.loadStmt.path.start + 1,
                                    node->as.loadStmt.path.length - 2);

    // 2. Push the raw string into the VM's constant pool
    emitConstant(OBJ_VAL(pathStr));

    // 3. Tell the VM to execute the load sequence!
    emitByte(OP_LOAD);
    break;
  }

  case NODE_UNION_TYPE: {
    for (int i = 0; i < node->as.unionType.count; i++) {
      walkNode(node->as.unionType.types[i]);
      current->temporaries++; // <-- TRACK EACH TYPE
    }

    emitByte(OP_BUILD_UNION);
    uint16_t count = (uint16_t)node->as.unionType.count;
    emitByte((count >> 8) & 0xff); // High byte
    emitByte(count & 0xff);        // Low byte

    // UNTRACK ALL TYPES
    current->temporaries -= node->as.unionType.count;
    break;
  }

  case NODE_INSTANTIATE: {
    walkNode(node->as.instantiate.target);
    current->temporaries++; // <-- TRACK THE TARGET BLUEPRINT/INSTANCE

    for (int i = 0; i < node->as.instantiate.count; i++) {
      Value nameVal =
          OBJ_VAL(copyString(node->as.instantiate.propertyNames[i].start,
                             node->as.instantiate.propertyNames[i].length));
      emitConstant(nameVal);
      current->temporaries++; // <-- TRACK PROPERTY NAME

      walkNode(node->as.instantiate.values[i]);
      current->temporaries++; // <-- TRACK PROPERTY VALUE
    }

    emitByte(OP_INSTANTIATE);
    uint16_t count = (uint16_t)node->as.instantiate.count;
    emitByte((count >> 8) & 0xff); // High byte
    emitByte(count & 0xff);        // Low byte

    // UNTRACK TARGET + ALL KEYS + ALL VALUES
    current->temporaries -= (1 + (node->as.instantiate.count * 2));
    break;
  }

  case NODE_COMPREHENSION: {
    // --- SCOPE 1: The Ghost Variables ---
    beginScope();

    // 1. Sequence
    walkNode(node->as.comprehension.sequence);
    addLocal(syntheticToken(" seq"));
    markInitialized();

    // 2. Iterator Setup
    emitByte(OP_GET_ITER);
    addLocal(syntheticToken(" iter"));
    markInitialized();

    // 3. The Accumulator!
    // OP_BUILD_LIST takes an initial count. We start empty, so we pass 0!
    if (node->as.comprehension.isDict) {
      emitByte(OP_BUILD_DICT);
      emitBytes(0, 0);
    } else {
      emitByte(OP_BUILD_LIST);
      emitBytes(0, 0);
    }
    addLocal(syntheticToken(" acc"));
    markInitialized();

    // 4. Mark Loop Top
    int loopStart = currentChunk()->count;
    CompilerLoop loop;
    startLoop(&loop, loopStart);

    // --- THE UPGRADED LOOP WIRING ---
    // Replace the old iterSlot and jump logic with this in BOTH places:
    Token iterTok = syntheticToken(" iter");
    int iterSlot = resolveLocal(current, &iterTok);

    emitByte(OP_FOR_ITER);
    emitByte((uint8_t)iterSlot);
    emitByte(0xff); // Placeholder for High Jump Byte
    emitByte(0xff); // Placeholder for Low Jump Byte
    int exitJump = currentChunk()->count - 2;

    // --- SCOPE 2: The User Variables ---
    beginScope();

    // 5. Register the Loop Variable (It's physically sitting on top of the
    // stack!)
    addLocal(node->as.comprehension.iterator);
    markInitialized();

    if (node->as.comprehension.hasIndex) {
      Token iterTok = syntheticToken(" iter");
      int iterSlot = resolveLocal(current, &iterTok);

      emitBytes(OP_GET_ITER_VALUE,
                (uint8_t)iterSlot); // <--- CHANGED FROM OP_GET_LOCAL!

      addLocal(node->as.comprehension.indexVar);
      markInitialized();
    }

    // 6. The Dual-Mode Execution
    if (node->as.comprehension.isBlockMode) {
      walkNode(node->as.comprehension.body);
    } else {
      // Expression Mode: 'keepValue' is now fully pre-packaged as a
      // NODE_KEEP (or a NODE_IF wrapping a NODE_KEEP).
      // We just walk it, and it will emit the correct opcodes automatically!
      walkNode(node->as.comprehension.keepValue);
    }

    // 7. Inner Cleanup
    endScope(); // Pops the user's iterator variables

    // 8. Loop mechanics
    emitLoop(loopStart);
    patchJump(exitJump);
    endLoop();

    // --- THE SCOPE SLIDE (Memory Magic) ---
    Token seqTok2 = syntheticToken(" seq");
    Token accTok2 = syntheticToken(" acc");
    int seqSlot = resolveLocal(current, &seqTok2);
    int accSlot = resolveLocal(current, &accTok2);

    // Copy the List/Dict from the 'acc' slot down into the 'seq' slot
    emitBytes(OP_GET_LOCAL, (uint8_t)accSlot);
    emitBytes(OP_SET_LOCAL, (uint8_t)seqSlot);
    emitByte(OP_POP); // Pop the value OP_SET_LOCAL left on the stack

    // Now physically pop the old 'acc' and 'iter' from the stack
    emitByte(OP_POP);
    emitByte(OP_POP);

    // Tell the compiler these variables no longer exist!
    current->localCount -= 3;
    current->scopeDepth--;
    break;
  }

  case NODE_KEEP: {
    Token accTok = syntheticToken(" acc");
    int accSlot = resolveLocal(current, &accTok);

    if (accSlot == -1) {
      // We use the Compiler's error function, not the VM's!
      error("I found a 'keep' statement outside of a comprehension.");
      break; // Abort code generation for this bad node
    }

    if (node->as.keepStmt.key != NULL) {
      walkNode(node->as.keepStmt.key);
      current->temporaries++; // <-- TRACK KEY
      walkNode(node->as.keepStmt.value);
      current->temporaries--; // <--  untrack key before op
      emitBytes(OP_KEEP_DICT, (uint8_t)accSlot);
    } else {
      walkNode(node->as.keepStmt.value);
      emitBytes(OP_KEEP_LIST, (uint8_t)accSlot);
    }
    break;
  }

  default:
    // Unhandled node type
    break;
  }
}

// ==========================================
// MAIN GENERATOR ENTRY
// ==========================================

ObjFunction *generateCode(Node *rootAST) {
  if (rootAST == NULL)
    return NULL;

  // 1. Boot up the compiler scope tracking
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);
  current = &compiler;

  // 2. Fire the Tree Walker!
  walkNode(rootAST);

  // 3. Wrap it up and return the finished VM function
  ObjFunction *function = endCompiler();

  return function;
}
