#include "ast.h"
#include "memory.h"
#include <stdlib.h>

// -- Compiling the dynamic arrays
DEFINE_ARRAY(Node *, NodeArray)
DEFINE_ARRAY(Token, TokenArray)

// Helper to allocate a raw node
static Node *allocateNode(NodeType type, int line) {
  Node *node = ALLOCATE(Node, 1);
  node->type = type;
  node->parent = NULL; // Default to NULL, parser links this later
  node->line = line;
  return node;
}

// --- Constructors ---

Node *newLiteralNode(Value value, int line) {
  Node *node = allocateNode(NODE_LITERAL, line);
  node->as.literal.value = value;
  return node;
}

Node *newBinaryNode(Node *left, Token operator, Node *right, int line) {
  Node *node = allocateNode(NODE_BINARY, line);
  node->as.binary.left = left;
  node->as.binary.opToken = operator;
  node->as.binary.right = right;

  // Wire up the parents! This is where the magic starts.
  if (left != NULL)
    left->parent = node;
  if (right != NULL)
    right->parent = node;

  return node;
}

Node *newIfNode(Node *condition, Node *thenBranch, Node *elseBranch, int line) {
  Node *node = allocateNode(NODE_IF, line);
  node->as.ifStmt.condition = condition;
  node->as.ifStmt.thenBranch = thenBranch;
  node->as.ifStmt.elseBranch = elseBranch;

  if (condition != NULL)
    condition->parent = node;
  if (thenBranch != NULL)
    thenBranch->parent = node;
  if (elseBranch != NULL)
    elseBranch->parent = node;

  return node;
}

Node *newVariableNode(Token name, int line) {
  Node *node = allocateNode(NODE_VARIABLE, line);
  node->as.variable.name = name;
  return node;
}

Node *newUnaryNode(Token opToken, Node *right, int line) {
  Node *node = allocateNode(NODE_UNARY, line);
  node->as.unary.opToken = opToken;
  node->as.unary.right = right;
  if (right != NULL)
    right->parent = node;
  return node;
}

Node *newRangeNode(Node *start, Node *end, Node *step, int line) {
  Node *node = allocateNode(NODE_RANGE, line);
  node->as.range.start = start;
  node->as.range.end = end;
  node->as.range.step = step;
  if (start)
    start->parent = node;
  if (end)
    end->parent = node;
  if (step)
    step->parent = node;
  return node;
}

Node *newListNode(Node **items, int count, int line) {
  Node *node = allocateNode(NODE_LIST, line);
  node->as.list.count = count;
  node->as.list.items = ALLOCATE(Node *, count);
  for (int i = 0; i < count; i++) {
    node->as.list.items[i] = items[i];
    if (items[i])
      items[i]->parent = node;
  }
  return node;
}

Node *newDictNode(Node **keys, Node **values, int count, int line) {
  // 1. Use the unified AST allocator
  Node *node = allocateNode(NODE_DICT, line);

  node->as.dictExpr.count = count;

  // 2. Use the VM's memory macros so the GC tracks these bytes!
  node->as.dictExpr.keys = ALLOCATE(Node *, count);
  node->as.dictExpr.values = ALLOCATE(Node *, count);

  for (int i = 0; i < count; i++) {
    node->as.dictExpr.keys[i] = keys[i];
    node->as.dictExpr.values[i] = values[i];
    if (keys[i])
      keys[i]->parent = node;
    if (values[i])
      values[i]->parent = node;
  }

  return node;
}

Node *newSubscriptNode(Node *left, Node *index, int line) {
  Node *node = allocateNode(NODE_SUBSCRIPT, line);
  node->as.subscript.left = left;
  node->as.subscript.index = index;
  if (left)
    left->parent = node;
  if (index)
    index->parent = node;
  return node;
}

Node *newEndNode(int line) { return allocateNode(NODE_END, line); }

Node *newBlockNode(Node **statements, int count, int line) {
  Node *node = allocateNode(NODE_BLOCK, line);
  node->as.block.count = count;
  node->as.block.statements = ALLOCATE(Node *, count);
  for (int i = 0; i < count; i++) {
    node->as.block.statements[i] = statements[i];
    if (statements[i] != NULL)
      statements[i]->parent = node;
  }
  return node;
}

Node *newSetNode(Node **targets, int targetCount, Node **values, int valueCount,
                 int line) {
  Node *node = allocateNode(NODE_SET, line);

  node->as.set.targetCount = targetCount;
  node->as.set.valueCount = valueCount;

  node->as.set.targets = ALLOCATE(Node *, targetCount);
  for (int i = 0; i < targetCount; i++) {
    node->as.set.targets[i] = targets[i];
    if (targets[i] != NULL)
      targets[i]->parent = node;
  }

  node->as.set.values = ALLOCATE(Node *, valueCount);
  for (int i = 0; i < valueCount; i++) {
    node->as.set.values[i] = values[i];
    if (values[i] != NULL)
      values[i]->parent = node;
  }

  return node;
}

Node *newSingleExprNode(NodeType type, Node *expression, int line) {
  Node *node = allocateNode(type, line); // Used for SHOW, RETURN, EXPR_STMT
  node->as.singleExpr.expression = expression;
  if (expression != NULL)
    expression->parent = node;
  return node;
}

Node *newLogicalNode(Node *left, Token opToken, Node *right, int line) {
  Node *node = allocateNode(NODE_LOGICAL, line);
  node->as.binary.left = left; // We can reuse the binary payload structurally
  node->as.binary.opToken = opToken;
  node->as.binary.right = right;

  if (left != NULL)
    left->parent = node;
  if (right != NULL)
    right->parent = node;

  return node;
}

Node *newWhileNode(Node *condition, Node *body, int line) {
  Node *node = allocateNode(NODE_WHILE, line);
  node->as.whileStmt.condition = condition;
  node->as.whileStmt.body = body;
  if (condition != NULL)
    condition->parent = node;
  if (body != NULL)
    body->parent = node;

  return node;
}

Node *newForNode(Token iterator, Node *sequence, Node *body, int line) {
  Node *node = allocateNode(NODE_FOR, line);
  node->as.forStmt.iterator = iterator;
  node->as.forStmt.sequence = sequence;
  node->as.forStmt.body = body;
  if (sequence != NULL)
    sequence->parent = node;
  if (body != NULL)
    body->parent = node;
  return node;
}

Node *newBreakNode(int line) {
  Node *node = allocateNode(NODE_BREAK, line);
  node->type = NODE_BREAK;
  node->parent = NULL;
  node->line = line;
  return node;
}

Node *newSkipNode(int line) {
  Node *node = allocateNode(NODE_SKIP, line);
  node->type = NODE_SKIP;
  node->parent = NULL;
  node->line = line;
  return node;
}

Node *newPhrasalCallNode(Token mangledName, Node **args, int argCount,
                         int line) {
  Node *node = allocateNode(NODE_PHRASAL_CALL, line);
  node->as.phrasalCall.mangledName = mangledName;
  node->as.phrasalCall.argCount = argCount;
  node->as.phrasalCall.arguments = ALLOCATE(Node *, argCount);

  for (int i = 0; i < argCount; i++) {
    node->as.phrasalCall.arguments[i] = args[i];
    if (args[i] != NULL)
      args[i]->parent = node;
  }
  return node;
}

Node *newLetNode(Token *names, int nameCount, Node **exprs, int exprCount,
                 int line) {
  Node *node = allocateNode(NODE_LET, line);

  node->as.let.nameCount = nameCount;
  node->as.let.exprCount = exprCount;

  node->as.let.names = ALLOCATE(Token, nameCount);
  node->as.let.exprs = ALLOCATE(Node *, exprCount);

  // Copy the variable names
  for (int i = 0; i < nameCount; i++) {
    node->as.let.names[i] = names[i];
  }

  // Copy the assigned expressions
  for (int i = 0; i < exprCount; i++) {
    node->as.let.exprs[i] = exprs[i];
    if (exprs[i] != NULL) {
      exprs[i]->parent = node;
    }
  }

  return node;
}

Node *newInterpolationNode(Node **parts, int partCount, int line) {
  Node *node = allocateNode(NODE_INTERPOLATION, line);
  node->as.interpolation.partCount = partCount;
  node->as.interpolation.parts = ALLOCATE(Node *, partCount);

  for (int i = 0; i < partCount; i++) {
    node->as.interpolation.parts[i] = parts[i];
    if (parts[i] != NULL)
      parts[i]->parent = node;
  }
  return node;
}

Node *newPropertyNode(Node *target, Token name, int line) {
  Node *node = allocateNode(NODE_PROPERTY, line);
  node->as.property.target = target;
  node->as.property.name = name;
  if (target != NULL)
    target->parent = node;
  return node;
}

Node *newCallNode(Node *callee, Node **arguments, int argCount, int line) {
  Node *node = allocateNode(NODE_CALL, line);
  node->as.call.callee = callee;
  node->as.call.argCount = argCount;
  node->as.call.arguments = ALLOCATE(Node *, argCount);
  for (int i = 0; i < argCount; i++) {
    node->as.call.arguments[i] = arguments[i];
    if (arguments[i] != NULL)
      arguments[i]->parent = node;
  }
  if (callee != NULL)
    callee->parent = node;
  return node;
}

Node *newFunctionNode(Token name, Token *parameters, Token *paramTypes,
                      int paramCount, Node *body, int line) {
  Node *node = allocateNode(NODE_FUNCTION, line);
  node->as.function.name = name;
  node->as.function.paramCount = paramCount;
  node->as.function.parameters = ALLOCATE(Token, paramCount);
  node->as.function.paramTypes = ALLOCATE(Token, paramCount); // NEW

  for (int i = 0; i < paramCount; i++) {
    node->as.function.parameters[i] = parameters[i];
    node->as.function.paramTypes[i] = paramTypes[i]; // NEW
  }
  node->as.function.body = body;
  if (body != NULL)
    body->parent = node;
  return node;
}

Node *newTypeNode(Token name, Token *propertyNames, Node **defaultValues,
                  int count, int line) {
  Node *node = allocateNode(NODE_TYPE_DECL, line);
  node->as.typeDecl.name = name;
  node->as.typeDecl.count = count;
  node->as.typeDecl.propertyNames = ALLOCATE(Token, count);
  node->as.typeDecl.defaultValues = ALLOCATE(Node *, count);

  for (int i = 0; i < count; i++) {
    node->as.typeDecl.propertyNames[i] = propertyNames[i];
    node->as.typeDecl.defaultValues[i] = defaultValues[i];
    if (defaultValues[i] != NULL)
      defaultValues[i]->parent = node;
  }
  return node;
}

Node *newInstantiateNode(Node *target, Token *propertyNames, Node **values,
                         int count, int line) {
  Node *node = allocateNode(NODE_INSTANTIATE, line);
  node->as.instantiate.target = target;
  node->as.instantiate.count = count;
  node->as.instantiate.propertyNames = ALLOCATE(Token, count);
  node->as.instantiate.values = ALLOCATE(Node *, count);

  if (target != NULL)
    target->parent = node;

  for (int i = 0; i < count; i++) {
    node->as.instantiate.propertyNames[i] = propertyNames[i];
    node->as.instantiate.values[i] = values[i];
    if (values[i] != NULL)
      values[i]->parent = node;
  }
  return node;
}

Node *newCastNode(Node *left, Node *right, int line) {
  Node *node = allocateNode(NODE_CAST, line);
  node->as.cast.left = left;
  node->as.cast.right = right;
  if (left)
    left->parent = node;
  if (right)
    right->parent = node;
  return node;
}

Node *newLoadNode(Token path, int line) {
  Node *node = allocateNode(NODE_LOAD, line);
  node->as.loadStmt.path = path;
  return node;
}

// --- The Destructor (Crucial for avoiding leaks) ---

void freeNode(Node *node) {
  if (node == NULL)
    return;

  // We must recursively free all children before freeing the parent!
  switch (node->type) {
  case NODE_BINARY:
  case NODE_LOGICAL: { // Reuses binary payload
    freeNode(node->as.binary.left);
    freeNode(node->as.binary.right);
    break;
  }
  case NODE_UNARY: {
    freeNode(node->as.unary.right);
    break;
  }
  case NODE_IF: {
    freeNode(node->as.ifStmt.condition);
    freeNode(node->as.ifStmt.thenBranch);
    freeNode(node->as.ifStmt.elseBranch);
    break;
  }
  case NODE_WHILE: {
    freeNode(node->as.whileStmt.condition);
    freeNode(node->as.whileStmt.body);
    break;
  }
  case NODE_FOR: {
    freeNode(node->as.forStmt.sequence);
    freeNode(node->as.forStmt.body);
    break;
  }
  case NODE_BLOCK: {
    for (int i = 0; i < node->as.block.count; i++) {
      freeNode(node->as.block.statements[i]);
    }
    FREE_ARRAY(Node *, node->as.block.statements, node->as.block.count);
    break;
  }
  case NODE_LET: {
    FREE_ARRAY(Token, node->as.let.names, node->as.let.nameCount);
    for (int i = 0; i < node->as.let.exprCount; i++) {
      freeNode(node->as.let.exprs[i]);
    }
    FREE_ARRAY(Node *, node->as.let.exprs, node->as.let.exprCount);
    break;
  }
  case NODE_SET: {
    for (int i = 0; i < node->as.set.targetCount; i++)
      freeNode(node->as.set.targets[i]);
    FREE_ARRAY(Node *, node->as.set.targets, node->as.set.targetCount);

    for (int i = 0; i < node->as.set.valueCount; i++)
      freeNode(node->as.set.values[i]);
    FREE_ARRAY(Node *, node->as.set.values, node->as.set.valueCount);
    break;
  }
  case NODE_LIST: {
    for (int i = 0; i < node->as.list.count; i++)
      freeNode(node->as.list.items[i]);
    FREE_ARRAY(Node *, node->as.list.items, node->as.list.count);
    break;
  }

  case NODE_DICT: {
    for (int i = 0; i < node->as.dictExpr.count; i++) {
      freeNode(node->as.dictExpr.keys[i]);
      freeNode(node->as.dictExpr.values[i]);
    }
    FREE_ARRAY(Node *, node->as.dictExpr.keys, node->as.dictExpr.count);
    FREE_ARRAY(Node *, node->as.dictExpr.values, node->as.dictExpr.count);
    break;
  }
  case NODE_INTERPOLATION: {
    for (int i = 0; i < node->as.interpolation.partCount; i++)
      freeNode(node->as.interpolation.parts[i]);
    FREE_ARRAY(Node *, node->as.interpolation.parts,
               node->as.interpolation.partCount);
    break;
  }
  case NODE_CALL: {
    freeNode(node->as.call.callee);
    for (int i = 0; i < node->as.call.argCount; i++)
      freeNode(node->as.call.arguments[i]);
    FREE_ARRAY(Node *, node->as.call.arguments, node->as.call.argCount);
    break;
  }
  case NODE_PHRASAL_CALL: {
    for (int i = 0; i < node->as.phrasalCall.argCount; i++)
      freeNode(node->as.phrasalCall.arguments[i]);
    FREE_ARRAY(Node *, node->as.phrasalCall.arguments,
               node->as.phrasalCall.argCount);
    free((void *)node->as.phrasalCall.mangledName.start);
    break;
  }
  case NODE_FUNCTION: {
    FREE_ARRAY(Token, node->as.function.parameters,
               node->as.function.paramCount);
    FREE_ARRAY(Token, node->as.function.paramTypes,
               node->as.function.paramCount); // NEW
    freeNode(node->as.function.body);
    free((void *)node->as.function.name.start);
    break;
  }
  case NODE_SUBSCRIPT: {
    freeNode(node->as.subscript.left);
    freeNode(node->as.subscript.index);
    break;
  }
  case NODE_PROPERTY: {
    freeNode(node->as.property.target);
    break;
  }
  case NODE_RANGE: {
    freeNode(node->as.range.start);
    freeNode(node->as.range.end);
    freeNode(node->as.range.step);
    break;
  }
  case NODE_EXPRESSION_STMT:
  case NODE_RETURN: {
    freeNode(node->as.singleExpr.expression);
    break;
  }

  case NODE_TYPE_DECL: {
    FREE_ARRAY(Token, node->as.typeDecl.propertyNames, node->as.typeDecl.count);
    for (int i = 0; i < node->as.typeDecl.count; i++) {
      freeNode(node->as.typeDecl.defaultValues[i]);
    }
    FREE_ARRAY(Node *, node->as.typeDecl.defaultValues,
               node->as.typeDecl.count);
    break;
  }

  case NODE_INSTANTIATE: {
    freeNode(node->as.instantiate.target);
    FREE_ARRAY(Token, node->as.instantiate.propertyNames,
               node->as.instantiate.count);
    for (int i = 0; i < node->as.instantiate.count; i++) {
      freeNode(node->as.instantiate.values[i]);
    }
    FREE_ARRAY(Node *, node->as.instantiate.values, node->as.instantiate.count);
    break;
  }

  case NODE_CAST: {
    freeNode(node->as.cast.left);
    freeNode(node->as.cast.right);
    break;
  }

  default:
    break; // Nodes like Literal, Variable, Break, Skip have no children/heap
           // arrays
  }

  // Finally, free the parent node itself
  FREE(Node, node);
}
