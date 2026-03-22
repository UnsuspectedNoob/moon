#include "ast.h"
#include "memory.h"
#include <stdlib.h>

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
  Node *node = malloc(sizeof(Node));
  node->type = NODE_DICT;
  node->line = line;
  node->parent = NULL;

  node->as.dictExpr.count = count;
  node->as.dictExpr.keys = malloc(sizeof(Node *) * count);
  node->as.dictExpr.values = malloc(sizeof(Node *) * count);

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

Node *newFunctionNode(Token name, Token *parameters, int paramCount, Node *body,
                      int line) {
  Node *node = allocateNode(NODE_FUNCTION, line);
  node->as.function.name = name;
  node->as.function.paramCount = paramCount;
  node->as.function.parameters = ALLOCATE(Token, paramCount);
  for (int i = 0; i < paramCount; i++) {
    node->as.function.parameters[i] = parameters[i];
  }
  node->as.function.body = body;
  if (body != NULL)
    body->parent = node;
  return node;
}

// --- The Destructor (Crucial for avoiding leaks) ---

void freeNode(Node *node) {
  if (node == NULL)
    return;

  // We must recursively free all children before freeing the parent!
  switch (node->type) {
  case NODE_BINARY: {
    freeNode(node->as.binary.left);
    freeNode(node->as.binary.right);
    break;
  }

  case NODE_IF: {
    freeNode(node->as.ifStmt.condition);
    freeNode(node->as.ifStmt.thenBranch);
    freeNode(node->as.ifStmt.elseBranch);
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
    // Free the names array
    FREE_ARRAY(Token, node->as.let.names, node->as.let.nameCount);

    // Walk and free the expressions based on exprCount
    for (int i = 0; i < node->as.let.exprCount; i++) {
      freeNode(node->as.let.exprs[i]);
    }

    // Free the expressions array
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

  // ... (We will expand this switch as we build out the other node types) ...
  default:
    break; // Some nodes (like Literal or Variable) have no children to free
  }

  // Finally, free the node itself
  FREE(Node, node);
}
