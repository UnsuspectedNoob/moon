#ifndef moon_ast_h
#define moon_ast_h

#include "memory.h"
#include "scanner.h"
#include "value.h"

// The absolute type of the node
typedef enum {
  NODE_LITERAL,
  NODE_VARIABLE,
  NODE_BINARY,
  NODE_UNARY,
  NODE_SUBSCRIPT,
  NODE_PROPERTY,
  NODE_CAST,
  NODE_CALL,
  NODE_PHRASAL_CALL,
  NODE_INTERPOLATION,
  NODE_EXPRESSION_STMT,
  NODE_LOGICAL,
  NODE_RETURN,
  NODE_SET,
  NODE_IF,
  NODE_WHILE,
  NODE_FOR,
  NODE_BREAK,
  NODE_SKIP,
  NODE_BLOCK,
  NODE_RANGE,
  NODE_LIST,
  NODE_DICT,
  NODE_LET,
  NODE_FUNCTION,
  NODE_TYPE_DECL,
  NODE_INSTANTIATE,
  NODE_END,
  NODE_LOAD,
  NODE_UNION_TYPE,
  NODE_COMPREHENSION,
  NODE_KEEP,
} NodeType;

// Forward declaration
typedef struct sNode Node;

// Dynamic Array Generators
DECLARE_ARRAY(Node *, NodeArray) // Generates NodeArray struct and functions!
DECLARE_ARRAY(Token, TokenArray) // Generates TokenArray struct and functions!

// --- Payload Structs ---
typedef struct {
  Value value;
} LiteralPayload;
typedef struct {
  Token name;
} VariablePayload;
typedef struct {
  Node *left;
  Token opToken;
  Node *right;
} BinaryPayload; // Changed operator to opToken
typedef struct {
  Token opToken;
  Node *right;
} UnaryPayload; // Changed operator to opToken
typedef struct {
  Node *start;
  Node *end;
  Node *step;
} RangePayload;

typedef struct {
  Node **items;
  int count;
} ListPayload;

typedef struct {
  Node **keys;
  Node **values;
  int count;
} DictPayload;

typedef struct {
  Node *left;
  Node *index;
} SubscriptPayload;
typedef struct {
  Node *target;
  Token name;
} PropertyPayload;
typedef struct {
  Node *callee;
  Node **arguments;
  int argCount;
} CallPayload;
typedef struct {
  Token mangledName;
  Node **arguments;
  int argCount;
} PhrasalCallPayload;
typedef struct {
  Node **parts;
  int partCount;
} InterpolationPayload;

typedef struct {
  Node *expression;
} SingleExprPayload;
typedef struct {
  Node **targets;
  int targetCount;
  Node **values;
  int valueCount;
} SetPayload;
typedef struct {
  Node *condition;
  Node *thenBranch;
  Node *elseBranch;
} IfPayload;
typedef struct {
  Node *condition;
  Node *body;
} WhilePayload;

typedef struct {
  Token iterator;
  Token indexVar; // <--- NEW: The second variable
  bool hasIndex;  // <--- NEW: Did they use a comma?
  Node *sequence;
  Node *body;
} ForPayload;

typedef struct {
  Node **statements;
  int count;
} BlockPayload;

typedef struct {
  Token *names;
  int nameCount;
  Node **exprs;
  int exprCount;
} LetPayload;

typedef struct {
  Token name;
  Token *parameters;
  Node **paramTypes;
  int paramCount;
  Node *body;
} FunctionPayload;

typedef struct {
  Token name;
  Token *propertyNames;
  Node **defaultValues;
  int count;
} TypeDeclPayload;

typedef struct {
  Node *target; // The expression evaluating to the Blueprint (e.g., 'Player')
  Token *propertyNames; // ["name", "health"]
  Node **values;        // ["Harry", 30]
  int count;
} InstantiatePayload;

typedef struct {
  Node *left;
  Node *right;
} CastPayload;

typedef struct {
  Token path;
} LoadPayload;

typedef struct {
  Node **types;
  int count;
} UnionTypePayload;

typedef struct {
  Token iterator;
  Token indexVar;
  bool hasIndex;
  Node *sequence;

  // Dual-Mode Fields
  bool isBlockMode;
  Node *keepValue; // Used in Expression Mode
  Node *keepKey;   // Used in Expression Mode (Dict only)
  Node *body;      // Used in Block Mode

  bool isDict; // Are we building a List or a Dict?
} ComprehensionPayload;

typedef struct {
  Node *key; // NULL if it's for a list!
  Node *value;
} KeepPayload;

// --- The Master Node ---
struct sNode {
  NodeType type;
  Node *parent;
  int line;

  union {
    LiteralPayload literal;
    VariablePayload variable;
    BinaryPayload binary;
    UnaryPayload unary;
    PropertyPayload property;
    CallPayload call;
    PhrasalCallPayload phrasalCall;
    InterpolationPayload interpolation;
    SingleExprPayload singleExpr;
    SetPayload set;
    IfPayload ifStmt;
    WhilePayload whileStmt;
    ForPayload forStmt;
    BlockPayload block;
    LetPayload let;
    FunctionPayload function;
    RangePayload range;
    CastPayload cast;
    ListPayload list;
    DictPayload dictExpr;
    SubscriptPayload subscript;
    TypeDeclPayload typeDecl;
    InstantiatePayload instantiate;
    LoadPayload loadStmt;
    UnionTypePayload unionType;
    ComprehensionPayload comprehension;
    KeepPayload keepStmt;
  } as;
};

// --- Constructors & Destructors ---
Node *newLiteralNode(Value value, int line);
Node *newVariableNode(Token name, int line);
Node *newUnaryNode(Token opToken, Node *right, int line);
Node *newBinaryNode(Node *left, Token opToken, Node *right, int line);
Node *newSubscriptNode(Node *target, Node *index, int line);
Node *newEndNode(int line);

Node *newBlockNode(Node **statements, int count, int line);
Node *newLetNode(Token *names, int nameCount, Node **exprs, int exprCount,
                 int line);
Node *newIfNode(Node *condition, Node *thenBranch, Node *elseBranch, int line);
Node *newSetNode(Node **targets, int targetCount, Node **values, int valueCount,
                 int line);
Node *newSingleExprNode(NodeType type, Node *expression, int line);
Node *newInterpolationNode(Node **parts, int partCount, int line);

Node *newLogicalNode(Node *left, Token opToken, Node *right, int line);
Node *newWhileNode(Node *condition, Node *body, int line);
Node *newForNode(Token iterator, Token indexVar, bool hasIndex, Node *sequence,
                 Node *body, int line);
Node *newBreakNode(int line);
Node *newSkipNode(int line);

Node *newPhrasalCallNode(Token mangledName, Node **args, int argCount,
                         int line);

Node *newListNode(Node **items, int count, int line);
Node *newDictNode(Node **keys, Node **values, int count, int line);

Node *newRangeNode(Node *start, Node *end, Node *step, int line);
Node *newPropertyNode(Node *target, Token name, int line);

Node *newCallNode(Node *callee, Node **arguments, int argCount, int line);

Node *newTypeNode(Token name, Token *propertyNames, Node **defaultValues,
                  int count, int line);
Node *newInstantiateNode(Node *target, Token *propertyNames, Node **values,
                         int count, int line);
Node *newCastNode(Node *left, Node *right, int line);
Node *newLoadNode(Token path, int line);
Node *newUnionTypeNode(Node **types, int count, int line);
Node *newFunctionNode(Token name, Token *parameters, Node **paramTypes,
                      int paramCount, Node *body,
                      int line); // <--- Updated signature
Node *newComprehensionNode(Token iterator, Token indexVar, bool hasIndex,
                           Node *sequence, bool isBlockMode, Node *keepValue,
                           Node *keepKey, Node *body, bool isDict, int line);
Node *newKeepNode(Node *key, Node *value, int line);

void freeNode(Node *node);

#endif
