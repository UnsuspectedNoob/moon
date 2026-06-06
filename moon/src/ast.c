#include "ast.h"
#include "memory.h"
#include <stdlib.h>
#include <string.h>

// -- Compiling the dynamic arrays
DEFINE_ARRAY(Node *, NodeArray)
DEFINE_ARRAY(Token, TokenArray)

// Helper to allocate a raw node
static Node *allocateNode(NodeType type, int line) {
  Node *node = ALLOCATE(Node, 1);
  memset(node, 0, sizeof(Node));
  node->type = type;
  node->parent = NULL; // Default to NULL, parser links this later
  node->line = line;
  node->usesIt = false;
  return node;
}

// --- Constructors ---

Node *newLiteralNode(Value value, int line) {
  Node *node = allocateNode(NODE_LITERAL, line);
  node->as.literal.value = value;
  return node;
}

Node *newBinaryNode(Node *left, Token opToken, Node *right, int line) {
  Node *node = ALLOCATE(Node, 1);
  node->type = NODE_BINARY;
  node->line = line;
  node->parent = NULL;
  node->usesIt = (left && left->usesIt) || (right && right->usesIt);
  node->as.binary.left = left;
  node->as.binary.opToken = opToken;
  node->as.binary.right = right;

  if (left != NULL)
    left->parent = node;
  if (right != NULL)
    right->parent = node;

  return node;
}

Node *newIfNode(Node *condition, Node *thenBranch, Node *elseBranch, int line) {
  Node *node = allocateNode(NODE_IF, line);
  if (condition != NULL && condition->usesIt) { node->usesIt = true; }
  if (thenBranch != NULL && thenBranch->usesIt) { node->usesIt = true; }
  if (elseBranch != NULL && elseBranch->usesIt) { node->usesIt = true; }
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
  if (name.length == 3 && name.start[0] == ' ' && name.start[1] == 'i' && name.start[2] == 't') {
    node->usesIt = true;
  }
  node->as.variable.name = name;
  return node;
}

Node *newUnaryNode(Token opToken, Node *right, int line) {
  Node *node = allocateNode(NODE_UNARY, line);
  if (right != NULL && right->usesIt) { node->usesIt = true; }
  node->as.unary.opToken = opToken;
  node->as.unary.right = right;
  if (right != NULL)
    right->parent = node;
  return node;
}

Node *newRangeNode(Node *start, Node *end, Node *step, int line) {
  Node *node = allocateNode(NODE_RANGE, line);
  if (start != NULL && start->usesIt) { node->usesIt = true; }
  if (end != NULL && end->usesIt) { node->usesIt = true; }
  if (step != NULL && step->usesIt) { node->usesIt = true; }
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
  if (items != NULL) { for (int i = 0; i < count; i++) { if (items[i] != NULL && items[i]->usesIt) node->usesIt = true; } }
  node->as.list.count = count;
  node->as.list.items = ALLOCATE(Node *, count);
  for (int i = 0; i < count; i++) {
    node->as.list.items[i] = items[i];
    if (items[i])
      items[i]->parent = node;
  }
  return node;
}

Node *newTupleNode(Node **items, int count, int line) {
  Node *node = allocateNode(NODE_TUPLE, line);
  if (items != NULL) { for (int i = 0; i < count; i++) { if (items[i] != NULL && items[i]->usesIt) node->usesIt = true; } }
  node->as.tuple.count = count;
  node->as.tuple.items = ALLOCATE(Node *, count);
  for (int i = 0; i < count; i++) {
    node->as.tuple.items[i] = items[i];
    if (items[i])
      items[i]->parent = node;
  }
  return node;
}

Node *newDictNode(Node **keys, Node **values, int count, int line) {
  // 1. Use the unified AST allocator
  Node *node = allocateNode(NODE_DICT, line);
  if (keys != NULL) { for (int i = 0; i < count; i++) { if (keys[i] != NULL && keys[i]->usesIt) node->usesIt = true; } }
  if (values != NULL) { for (int i = 0; i < count; i++) { if (values[i] != NULL && values[i]->usesIt) node->usesIt = true; } }

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
  if (left != NULL && left->usesIt) { node->usesIt = true; }
  if (index != NULL && index->usesIt) { node->usesIt = true; }
  node->as.subscript.left = left;
  node->as.subscript.index = index;
  if (left)
    left->parent = node;
  if (index)
    index->parent = node;
  return node;
}

Node *newEndNode(int line) { return allocateNode(NODE_END, line); }
Node *newLoadStickyNode(int line) { return allocateNode(NODE_LOAD_STICKY, line); }

Node *newBlockNode(Node **statements, int count, int line) {
  Node *node = allocateNode(NODE_BLOCK, line);
  if (statements != NULL) { for (int i = 0; i < count; i++) { if (statements[i] != NULL && statements[i]->usesIt) node->usesIt = true; } }
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
  if (targets != NULL) { for (int i = 0; i < targetCount; i++) { if (targets[i] != NULL && targets[i]->usesIt) node->usesIt = true; } }
  if (values != NULL) { for (int i = 0; i < valueCount; i++) { if (values[i] != NULL && values[i]->usesIt) node->usesIt = true; } }

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
  if (expression != NULL && expression->usesIt) { node->usesIt = true; }
  node->as.singleExpr.expression = expression;
  if (expression != NULL)
    expression->parent = node;
  return node;
}

Node *newLogicalNode(Node *left, Token opToken, Node *right, int line) {
  Node *node = allocateNode(NODE_LOGICAL, line);
  if (left != NULL && left->usesIt) { node->usesIt = true; }
  if (right != NULL && right->usesIt) { node->usesIt = true; }
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
  if (condition != NULL && condition->usesIt) { node->usesIt = true; }
  if (body != NULL && body->usesIt) { node->usesIt = true; }
  node->as.whileStmt.condition = condition;
  node->as.whileStmt.body = body;
  if (condition != NULL)
    condition->parent = node;
  if (body != NULL)
    body->parent = node;

  return node;
}

Node *newForNode(Token iterator, Token indexVar, bool hasIndex, Node *sequence,
                 Node *body, int line) {
  Node *node = allocateNode(NODE_FOR, line);
  if (sequence != NULL && sequence->usesIt) { node->usesIt = true; }
  node->as.forStmt.iterator = iterator;
  node->as.forStmt.indexVar = indexVar; // <--- NEW
  node->as.forStmt.hasIndex = hasIndex; // <--- NEW
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
  node->usesIt = false;
  return node;
}

Node *newSkipNode(int line) {
  Node *node = allocateNode(NODE_SKIP, line);
  node->type = NODE_SKIP;
  node->parent = NULL;
  node->line = line;
  node->usesIt = false;
  return node;
}

Node *newPhrasalCallNode(Token mangledName, Node **args, int argCount,
                         Token *phraseTokens, int phraseTokenCount,
                         int line) {
  Node *node = allocateNode(NODE_PHRASAL_CALL, line);
  if (args != NULL) { for (int i = 0; i < argCount; i++) { if (args[i] != NULL && args[i]->usesIt) node->usesIt = true; } }
  node->as.phrasalCall.mangledName = mangledName;
  node->as.phrasalCall.argCount = argCount;
  node->as.phrasalCall.arguments = ALLOCATE(Node *, argCount);
  node->as.phrasalCall.phraseTokenCount = phraseTokenCount;
  node->as.phrasalCall.phraseTokens = ALLOCATE(Token, phraseTokenCount);

  for (int i = 0; i < phraseTokenCount; i++) {
    node->as.phrasalCall.phraseTokens[i] = phraseTokens[i];
  }

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
  if (exprs != NULL) { for (int i = 0; i < exprCount; i++) { if (exprs[i] != NULL && exprs[i]->usesIt) node->usesIt = true; } }

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
  if (parts != NULL) { for (int i = 0; i < partCount; i++) { if (parts[i] != NULL && parts[i]->usesIt) node->usesIt = true; } }
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
  if (target != NULL && target->usesIt) { node->usesIt = true; }
  node->as.property.target = target;
  node->as.property.name = name;
  if (target != NULL)
    target->parent = node;
  return node;
}


Node *newFunctionNode(Token name, Token *parameters, Node **paramTypes,
                      int paramCount, Node *body, int line) {
  Node *node = allocateNode(NODE_FUNCTION, line);
  node->as.function.name = name;
  node->as.function.paramCount = paramCount;
  node->as.function.parameters = ALLOCATE(Token, paramCount);
  node->as.function.paramTypes =
      ALLOCATE(Node *, paramCount); // Now allocates Node Pointers!

  for (int i = 0; i < paramCount; i++) {
    node->as.function.parameters[i] = parameters[i];
    node->as.function.paramTypes[i] = paramTypes[i];
    if (paramTypes[i] != NULL)
      paramTypes[i]->parent = node; // Link the parent!
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
  if (target != NULL && target->usesIt) { node->usesIt = true; }
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
  if (left != NULL && left->usesIt) { node->usesIt = true; }
  if (right != NULL && right->usesIt) { node->usesIt = true; }
  node->as.cast.left = left;
  node->as.cast.right = right;
  if (left)
    left->parent = node;
  if (right)
    right->parent = node;
  return node;
}



Node *newChainNode(Node **expressions, Token *operators, int exprCount, int line) {
  Node *node = ALLOCATE(Node, 1);
  node->type = NODE_CHAIN;
  node->line = line;
  node->parent = NULL;
  
  bool usesIt = false;
  for (int i = 0; i < exprCount; i++) {
    if (expressions[i]) {
      expressions[i]->parent = node;
      if (expressions[i]->usesIt) {
        usesIt = true;
      }
    }
  }
  node->usesIt = usesIt;

  node->as.chain.expressions = expressions;
  node->as.chain.operators = operators;
  node->as.chain.exprCount = exprCount;

  return node;
}

Node *newLoadNode(Token path, int line) {
  Node *node = allocateNode(NODE_LOAD, line);
  node->as.loadStmt.path = path;
  return node;
}

Node *newUnionTypeNode(Node **types, int count, int line) {
  Node *node = allocateNode(NODE_UNION_TYPE, line);
  if (types != NULL) { for (int i = 0; i < count; i++) { if (types[i] != NULL && types[i]->usesIt) node->usesIt = true; } }
  node->as.unionType.count = count;
  node->as.unionType.types = ALLOCATE(Node *, count);
  for (int i = 0; i < count; i++) {
    node->as.unionType.types[i] = types[i];
    if (types[i] != NULL)
      types[i]->parent = node;
  }
  return node;
}

Node *newComprehensionNode(Token iterator, Token indexVar, bool hasIndex,
                           Node *sequence, Node *body, bool isDict, int line) {
  Node *node = allocateNode(NODE_COMPREHENSION, line);
  if (sequence != NULL && sequence->usesIt) node->usesIt = true;
  if (body != NULL && body->usesIt) node->usesIt = true;

  node->as.comprehension.iterator = iterator;
  node->as.comprehension.indexVar = indexVar;
  node->as.comprehension.hasIndex = hasIndex;
  node->as.comprehension.sequence = sequence;
  node->as.comprehension.body = body;
  node->as.comprehension.isDict = isDict;

  if (sequence != NULL)
    sequence->parent = node;
  if (body != NULL)
    body->parent = node;
  return node;
}

Node *newKeepNode(Node *key, Node *value, int line) {
  Node *node = allocateNode(NODE_KEEP, line);
  if (key != NULL && key->usesIt) { node->usesIt = true; }
  if (value != NULL && value->usesIt) { node->usesIt = true; }
  node->as.keepStmt.key = key;
  node->as.keepStmt.value = value;
  if (key)
    key->parent = node;
  if (value)
    value->parent = node;
  return node;
}

// --- The Destructor (Crucial for avoiding leaks) ---

void freeNode(Node *root) {
  if (root == NULL)
    return;

  // 1. Initialize our custom heap-allocated stack
  NodeArray worklist;
  initNodeArray(&worklist);
  writeNodeArray(&worklist, root);

  // 2. Iteratively process until the tree is completely destroyed
  while (worklist.count > 0) {
    // Pop the top node off our worklist
    Node *node = worklist.items[--worklist.count];

    if (node == NULL)
      continue;

    // STEP A: Push all children to the worklist so they get processed next
    switch (node->type) {
    case NODE_BINARY:
    case NODE_LOGICAL:
      writeNodeArray(&worklist, node->as.binary.left);
      writeNodeArray(&worklist, node->as.binary.right);
      break;
    case NODE_UNARY:
      writeNodeArray(&worklist, node->as.unary.right);
      break;
    case NODE_CHAIN:
      for (int i = 0; i < node->as.chain.exprCount; i++)
        writeNodeArray(&worklist, node->as.chain.expressions[i]);
      break;
    case NODE_IF:
      writeNodeArray(&worklist, node->as.ifStmt.condition);
      writeNodeArray(&worklist, node->as.ifStmt.thenBranch);
      writeNodeArray(&worklist, node->as.ifStmt.elseBranch);
      break;
    case NODE_WHILE:
      writeNodeArray(&worklist, node->as.whileStmt.condition);
      writeNodeArray(&worklist, node->as.whileStmt.body);
      break;
    case NODE_FOR:
      writeNodeArray(&worklist, node->as.forStmt.sequence);
      writeNodeArray(&worklist, node->as.forStmt.body);
      break;
    case NODE_BLOCK:
      for (int i = 0; i < node->as.block.count; i++)
        writeNodeArray(&worklist, node->as.block.statements[i]);
      break;
    case NODE_LET:
      for (int i = 0; i < node->as.let.exprCount; i++)
        writeNodeArray(&worklist, node->as.let.exprs[i]);
      break;
    case NODE_SET:
      for (int i = 0; i < node->as.set.targetCount; i++)
        writeNodeArray(&worklist, node->as.set.targets[i]);
      for (int i = 0; i < node->as.set.valueCount; i++)
        writeNodeArray(&worklist, node->as.set.values[i]);
      break;
    case NODE_LIST:
      for (int i = 0; i < node->as.list.count; i++)
        writeNodeArray(&worklist, node->as.list.items[i]);
      break;
    case NODE_TUPLE:
      for (int i = 0; i < node->as.tuple.count; i++)
        writeNodeArray(&worklist, node->as.tuple.items[i]);
      break;
    case NODE_DICT:
      for (int i = 0; i < node->as.dictExpr.count; i++) {
        writeNodeArray(&worklist, node->as.dictExpr.keys[i]);
        writeNodeArray(&worklist, node->as.dictExpr.values[i]);
      }
      break;
    case NODE_INTERPOLATION:
      for (int i = 0; i < node->as.interpolation.partCount; i++)
        writeNodeArray(&worklist, node->as.interpolation.parts[i]);
      break;
    
    case NODE_PHRASAL_CALL:
      for (int i = 0; i < node->as.phrasalCall.argCount; i++)
        writeNodeArray(&worklist, node->as.phrasalCall.arguments[i]);
      break;
    case NODE_UNION_TYPE:
      for (int i = 0; i < node->as.unionType.count; i++)
        writeNodeArray(&worklist, node->as.unionType.types[i]);
      break;
    case NODE_FUNCTION:
      writeNodeArray(&worklist, node->as.function.body);
      // Push the new type nodes to the GC worklist!
      for (int i = 0; i < node->as.function.paramCount; i++)
        writeNodeArray(&worklist, node->as.function.paramTypes[i]);
      break;
    case NODE_SUBSCRIPT:
      writeNodeArray(&worklist, node->as.subscript.left);
      writeNodeArray(&worklist, node->as.subscript.index);
      break;
    case NODE_PROPERTY:
      writeNodeArray(&worklist, node->as.property.target);
      break;
    case NODE_RANGE:
      writeNodeArray(&worklist, node->as.range.start);
      writeNodeArray(&worklist, node->as.range.end);
      writeNodeArray(&worklist, node->as.range.step);
      break;
    case NODE_EXPRESSION_STMT:
    case NODE_RETURN:
    case NODE_GROUPING:
    case NODE_BIND_STICKY:
      writeNodeArray(&worklist, node->as.singleExpr.expression);
      break;
    case NODE_TYPE_DECL:
      for (int i = 0; i < node->as.typeDecl.count; i++)
        writeNodeArray(&worklist, node->as.typeDecl.defaultValues[i]);
      break;
    case NODE_INSTANTIATE:
      writeNodeArray(&worklist, node->as.instantiate.target);
      for (int i = 0; i < node->as.instantiate.count; i++)
        writeNodeArray(&worklist, node->as.instantiate.values[i]);
      break;
    case NODE_CAST:
      writeNodeArray(&worklist, node->as.cast.left);
      writeNodeArray(&worklist, node->as.cast.right);
      break;
    case NODE_COMPREHENSION:
      writeNodeArray(&worklist, node->as.comprehension.sequence);
      writeNodeArray(&worklist, node->as.comprehension.body);
      break;
    case NODE_KEEP:
      writeNodeArray(&worklist, node->as.keepStmt.key);
      writeNodeArray(&worklist, node->as.keepStmt.value);
      break;
    default:
      // Leaf nodes have no children to push
      break;
    }

    // STEP B: Free the node's specific dynamically allocated C-arrays
    switch (node->type) {
    case NODE_BLOCK:
      FREE_ARRAY(Node *, node->as.block.statements, node->as.block.count);
      break;
    case NODE_CHAIN:
      FREE_ARRAY(Node *, node->as.chain.expressions, node->as.chain.exprCount);
      FREE_ARRAY(Token, node->as.chain.operators, node->as.chain.exprCount - 1);
      break;
    case NODE_LET:
      FREE_ARRAY(Token, node->as.let.names, node->as.let.nameCount);
      FREE_ARRAY(Node *, node->as.let.exprs, node->as.let.exprCount);
      break;
    case NODE_SET:
      FREE_ARRAY(Node *, node->as.set.targets, node->as.set.targetCount);
      FREE_ARRAY(Node *, node->as.set.values, node->as.set.valueCount);
      break;
    case NODE_LIST:
      FREE_ARRAY(Node *, node->as.list.items, node->as.list.count);
      break;
    case NODE_TUPLE:
      FREE_ARRAY(Node *, node->as.tuple.items, node->as.tuple.count);
      break;
    case NODE_DICT:
      FREE_ARRAY(Node *, node->as.dictExpr.keys, node->as.dictExpr.count);
      FREE_ARRAY(Node *, node->as.dictExpr.values, node->as.dictExpr.count);
      break;
    case NODE_INTERPOLATION:
      FREE_ARRAY(Node *, node->as.interpolation.parts,
                 node->as.interpolation.partCount);
      break;
    
    case NODE_PHRASAL_CALL:
      FREE_ARRAY(Node *, node->as.phrasalCall.arguments,
                 node->as.phrasalCall.argCount);
      free((void *)node->as.phrasalCall.mangledName.start);
      break;
    case NODE_UNION_TYPE:
      FREE_ARRAY(Node *, node->as.unionType.types, node->as.unionType.count);
      break;
    case NODE_FUNCTION:
      FREE_ARRAY(Token, node->as.function.parameters,
                 node->as.function.paramCount);
      FREE_ARRAY(Node *, node->as.function.paramTypes,
                 node->as.function.paramCount); // Free Node* array!
      free((void *)node->as.function.name.start);
      break;
    case NODE_TYPE_DECL:
      FREE_ARRAY(Token, node->as.typeDecl.propertyNames,
                 node->as.typeDecl.count);
      FREE_ARRAY(Node *, node->as.typeDecl.defaultValues,
                 node->as.typeDecl.count);
      break;
    case NODE_INSTANTIATE:
      FREE_ARRAY(Token, node->as.instantiate.propertyNames,
                 node->as.instantiate.count);
      FREE_ARRAY(Node *, node->as.instantiate.values,
                 node->as.instantiate.count);
      break;
    default:
      break;
    }

    // STEP C: Free the node wrapper itself
    FREE(Node, node);
  }

  // 3. Clean up our worklist
  freeNodeArray(&worklist);
}
