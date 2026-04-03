#include <stdio.h>

#include "ast.h"
#include "debug.h"
#include "value.h"
#include "vm.h"

// 1. The Stack Visualizer
void debugStack(VM *vm) {
  printf("          "); // Indent to align with opcode column

  // Loop from bottom (vm->stack) to top (vm->stackTop)
  for (Value *slot = vm->stack; slot < vm->stackTop; slot++) {
    printf("[ ");
    printValue(*slot);
    printf(" ]");
  }

  printf("\n");
}

void disassembleChunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);

  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static int simpleInstruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int byteInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t slot = chunk->code[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}

static int shortInstruction(const char *name, Chunk *chunk, int offset) {
  uint16_t slot = (uint16_t)(chunk->code[offset + 1] << 8);
  slot |= chunk->code[offset + 2];
  printf("%-16s %4d\n", name, slot);
  return offset + 3;
}

static int jumpInstruction(const char *name, int sign, Chunk *chunk,
                           int offset) {
  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
  jump |= chunk->code[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
}

static int constantInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 2;
}

static int constantLongInstruction(const char *name, Chunk *chunk, int offset) {
  uint16_t constant = (uint16_t)(chunk->code[offset + 1] << 8);
  constant |= chunk->code[offset + 2];
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 3;
}

static int typeInstruction(const char *name, Chunk *chunk, int offset) {
  uint16_t nameConst =
      (uint16_t)(chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
  uint16_t count =
      (uint16_t)(chunk->code[offset + 3] << 8) | chunk->code[offset + 4];
  printf("%-16s %4d '", name, nameConst);
  printValue(chunk->constants.values[nameConst]);
  printf("' (%d properties)\n", count);
  return offset + 5;
}

int disassembleInstruction(Chunk *chunk, int offset) {
  printf("%04d ", offset);
  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", chunk->lines[offset]);
  }

  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
  case OP_CONSTANT:
    return constantInstruction("OP_CONSTANT", chunk, offset);
  case OP_CONSTANT_LONG:
    return constantLongInstruction("OP_CONSTANT_LONG", chunk, offset);
  case OP_NIL:
    return simpleInstruction("OP_NIL", offset);
  case OP_TRUE:
    return simpleInstruction("OP_TRUE", offset);
  case OP_FALSE:
    return simpleInstruction("OP_FALSE", offset);
  case OP_POP:
    return simpleInstruction("OP_POP", offset);
  case OP_GET_LOCAL:
    return byteInstruction("OP_GET_LOCAL", chunk, offset);
  case OP_SET_LOCAL:
    return byteInstruction("OP_SET_LOCAL", chunk, offset);
  case OP_GET_GLOBAL:
    return constantLongInstruction("OP_GET_GLOBAL", chunk, offset);
  case OP_DEFINE_GLOBAL:
    return constantLongInstruction("OP_DEFINE_GLOBAL", chunk, offset);
  case OP_SET_GLOBAL:
    return constantLongInstruction("OP_SET_GLOBAL", chunk, offset);
  case OP_EQUAL:
    return simpleInstruction("OP_EQUAL", offset);
  case OP_GREATER:
    return simpleInstruction("OP_GREATER", offset);
  case OP_LESS:
    return simpleInstruction("OP_LESS", offset);
  case OP_ADD:
    return simpleInstruction("OP_ADD", offset);
  case OP_ADD_INPLACE:
    return simpleInstruction("OP_ADD_INPLACE", offset);
  case OP_SUBTRACT:
    return simpleInstruction("OP_SUBTRACT", offset);
  case OP_MULTIPLY:
    return simpleInstruction("OP_MULTIPLY", offset);
  case OP_DIVIDE:
    return simpleInstruction("OP_DIVIDE", offset);
  case OP_NOT:
    return simpleInstruction("OP_NOT", offset);
  case OP_NEGATE:
    return simpleInstruction("OP_NEGATE", offset);

  // NEW JUMP DEBUGGING:
  case OP_JUMP:
    return jumpInstruction("OP_JUMP", 1, chunk, offset);
  case OP_JUMP_IF_FALSE:
    return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
  case OP_LOOP:
    return jumpInstruction("OP_LOOP", -1, chunk, offset);

  case OP_BUILD_STRING:
    return shortInstruction("OP_BUILD_STRING", chunk, offset);
  case OP_BUILD_LIST:
    return shortInstruction("OP_BUILD_LIST", chunk, offset);
  case OP_BUILD_DICT:
    return shortInstruction("OP_BUILD_DICT", chunk, offset);
  case OP_CALL:
    return shortInstruction("OP_CALL", chunk, offset);

  case OP_GET_PROPERTY:
    return constantLongInstruction("OP_GET_PROPERTY", chunk, offset);
  case OP_SET_PROPERTY:
    return constantLongInstruction("OP_SET_PROPERTY", chunk, offset);

  case OP_GET_SUBSCRIPT:
    return simpleInstruction("OP_GET_SUBSCRIPT", offset);
  case OP_SET_SUBSCRIPT:
    return simpleInstruction("OP_SET_SUBSCRIPT", offset);
  case OP_GET_END_INDEX:
    return simpleInstruction("OP_GET_END_INDEX", offset);
  case OP_RANGE:
    return simpleInstruction("OP_RANGE", offset);
    // ...

  case OP_FOR_ITER:
    return jumpInstruction("OP_FOR_ITER", 1, chunk, offset);
  case OP_GET_ITER: // <--- Add this
    return simpleInstruction("OP_GET_ITER", offset);
  case OP_CAST:
    return simpleInstruction("OP_CAST", offset);
  case OP_TYPE_DEF:
    return typeInstruction("OP_TYPE_DEF", chunk, offset);
  case OP_INSTANTIATE:
    return shortInstruction("OP_INSTANTIATE", chunk, offset);
  case OP_DEFINE_METHOD:
    return constantLongInstruction("OP_DEFINE_METHOD", chunk, offset);

  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);

  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}

void printAST(Node *node, int indent) {
  if (node == NULL)
    return;

  for (int i = 0; i < indent; i++) {
    printf((i == indent - 1) ? " ├─ " : " │  ");
  }

  switch (node->type) {
  case NODE_LITERAL:
    printf("[LITERAL: ");
    printValue(node->as.literal.value);
    printf("]\n");
    break;

  case NODE_VARIABLE:
    printf("[VARIABLE: %.*s]\n", node->as.variable.name.length,
           node->as.variable.name.start);
    break;

  case NODE_UNARY:
    printf("[UNARY: %.*s]\n", node->as.unary.opToken.length,
           node->as.unary.opToken.start);
    printAST(node->as.unary.right, indent + 1);
    break;

  case NODE_BINARY:
  case NODE_LOGICAL:
    printf("[%s: %.*s]\n", node->type == NODE_BINARY ? "BINARY" : "LOGICAL",
           node->as.binary.opToken.length, node->as.binary.opToken.start);
    printAST(node->as.binary.left, indent + 1);
    printAST(node->as.binary.right, indent + 1);
    break;

  case NODE_BLOCK:
    printf("[BLOCK: %d statements]\n", node->as.block.count);
    for (int i = 0; i < node->as.block.count; i++) {
      printAST(node->as.block.statements[i], indent + 1);
    }
    break;

  case NODE_IF:
    printf("[IF STATEMENT]\n");
    printAST(node->as.ifStmt.condition, indent + 1);
    printf("%*s ├─ [THEN]\n", indent * 4, "");
    printAST(node->as.ifStmt.thenBranch, indent + 2);
    if (node->as.ifStmt.elseBranch) {
      printf("%*s ├─ [ELSE]\n", indent * 4, "");
      printAST(node->as.ifStmt.elseBranch, indent + 2);
    }
    break;

  case NODE_LET:
    printf("[LET DECLARATION: %d variables]\n", node->as.let.nameCount);
    for (int i = 0; i < node->as.let.nameCount; i++) {
      printf("%*s ├─ Var: %.*s\n", indent * 4, "", node->as.let.names[i].length,
             node->as.let.names[i].start);
    }
    for (int i = 0; i < node->as.let.exprCount; i++) {
      printAST(node->as.let.exprs[i], indent + 1);
    }
    break;

  case NODE_SET:
    printf("[SET ASSIGNMENT]\n");
    for (int i = 0; i < node->as.set.targetCount; i++) {
      printAST(node->as.set.targets[i], indent + 1);
    }
    printf("%*s ├─ [TO]\n", indent * 4, "");
    for (int i = 0; i < node->as.set.valueCount; i++) {
      printAST(node->as.set.values[i], indent + 2);
    }
    break;

  case NODE_PHRASAL_CALL:
    printf("[PHRASAL CALL: %s]\n", node->as.phrasalCall.mangledName.start);
    for (int i = 0; i < node->as.phrasalCall.argCount; i++) {
      printAST(node->as.phrasalCall.arguments[i], indent + 1);
    }
    break;

  case NODE_RETURN:
    printf("[GIVE (RETURN)]\n");
    printAST(node->as.singleExpr.expression, indent + 1);
    break;

  case NODE_EXPRESSION_STMT:
    printf("[EXPR STMT]\n");
    printAST(node->as.singleExpr.expression, indent + 1);
    break;

  case NODE_LIST:
    printf("[LIST: %d items]\n", node->as.list.count);
    for (int i = 0; i < node->as.list.count; i++) {
      printAST(node->as.list.items[i], indent + 1);
    }
    break;

  case NODE_DICT:
    printf("[DICTIONARY: %d pairs]\n", node->as.dictExpr.count);
    for (int i = 0; i < node->as.dictExpr.count; i++) {
      for (int j = 0; j < indent + 1; j++)
        printf(" |  ");
      printf("├─ [PAIR]\n");
      printAST(node->as.dictExpr.keys[i], indent + 2);
      printAST(node->as.dictExpr.values[i], indent + 2);
    }
    break;

  case NODE_SUBSCRIPT:
    printf("[SUBSCRIPT]\n");
    printAST(node->as.subscript.left, indent + 1);
    printf("%*s ├─ [INDEX]\n", indent * 4, "");
    printAST(node->as.subscript.index, indent + 2);
    break;

  case NODE_RANGE:
    printf("[RANGE]\n");
    printAST(node->as.range.start, indent + 1);
    printf("%*s ├─ [TO]\n", indent * 4, "");
    printAST(node->as.range.end, indent + 2);
    printf("%*s ├─ [BY]\n", indent * 4, "");
    printAST(node->as.range.step, indent + 2);
    break;

  case NODE_END:
    printf("[KEYWORD: end]\n");
    break;

  case NODE_FUNCTION:
    printf("[FUNCTION: %.*s]\n", node->as.function.name.length,
           node->as.function.name.start);
    for (int i = 0; i < node->as.function.paramCount; i++) {
      // THE UPGRADE: It prints both the parameter name AND its extracted Type
      // Annotation!
      printf("%*s ├─ Param: %.*s (Type: %.*s)\n", indent * 4, "",
             node->as.function.parameters[i].length,
             node->as.function.parameters[i].start,
             node->as.function.paramTypes[i].length,
             node->as.function.paramTypes[i].start);
    }
    printf("%*s ├─ [BODY]\n", indent * 4, "");
    printAST(node->as.function.body, indent + 2);
    break;

  case NODE_CALL:
    printf("[CALL]\n");
    printAST(node->as.call.callee, indent + 1);
    for (int i = 0; i < node->as.call.argCount; i++) {
      printAST(node->as.call.arguments[i], indent + 2);
    }
    break;

  case NODE_PROPERTY:
    printf("[PROPERTY ACCESS: %.*s]\n", node->as.property.name.length,
           node->as.property.name.start);
    printAST(node->as.property.target, indent + 1);
    break;

  case NODE_WHILE:
    printf("[WHILE LOOP]\n");
    printAST(node->as.whileStmt.condition, indent + 1);
    printf("%*s ├─ [BODY]\n", indent * 4, "");
    printAST(node->as.whileStmt.body, indent + 2);
    break;

  case NODE_FOR:
    printf("[FOR LOOP: iterator %.*s]\n", node->as.forStmt.iterator.length,
           node->as.forStmt.iterator.start);
    printAST(node->as.forStmt.sequence, indent + 1);
    printf("%*s ├─ [BODY]\n", indent * 4, "");
    printAST(node->as.forStmt.body, indent + 2);
    break;

  case NODE_BREAK:
    printf("[BREAK]\n");
    break;

  case NODE_SKIP:
    printf("[SKIP (CONTINUE)]\n");
    break;

  case NODE_INTERPOLATION:
    printf("[INTERPOLATION: %d parts]\n", node->as.interpolation.partCount);
    for (int i = 0; i < node->as.interpolation.partCount; i++) {
      printAST(node->as.interpolation.parts[i], indent + 1);
    }
    break;

  case NODE_TYPE_DECL:
    printf("[TYPE BLUEPRINT: %.*s]\n", node->as.typeDecl.name.length,
           node->as.typeDecl.name.start);
    for (int i = 0; i < node->as.typeDecl.count; i++) {
      printf("%*s ├─ Property: %.*s\n", indent * 4, "",
             node->as.typeDecl.propertyNames[i].length,
             node->as.typeDecl.propertyNames[i].start);
      printAST(node->as.typeDecl.defaultValues[i], indent + 2);
    }
    break;

  case NODE_CAST:
    printf("[CAST]\n");
    printAST(node->as.cast.left, indent + 1);
    printf("%*s ├─ [AS]\n", indent * 4, "");
    printAST(node->as.cast.right, indent + 2);
    break;

  case NODE_INSTANTIATE:
    printf("[INSTANTIATE CLONE]\n");
    printAST(node->as.instantiate.target, indent + 1);
    for (int i = 0; i < node->as.instantiate.count; i++) {
      printf("%*s ├─ Override: %.*s\n", indent * 4, "",
             node->as.instantiate.propertyNames[i].length,
             node->as.instantiate.propertyNames[i].start);
      printAST(node->as.instantiate.values[i], indent + 2);
    }
    break;

  default:
    printf("[UNKNOWN NODE TYPE: %d]\n", node->type);
    break;
  }
}
