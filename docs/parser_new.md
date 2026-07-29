# Documentation: `parser.h`

## Overview
- **Purpose**: [INSERT MODULE PURPOSE HERE]

## Structs
### `Parser`
- **Fields**: `Token current, Token previous, bool hadError, bool panicMode`
- **Description**: [INSERT DESCRIPTION HERE]

## Global/Static Variables
### `extern Parser parser;`
- **Description**: [INSERT DESCRIPTION HERE]

## Functions
### `Node *parseSource(const char *source, int startLine)`
- **Description**: [INSERT DESCRIPTION HERE]

### `void errorAt(Token *token, ErrorType type, const char *message,
             const char *hint)`
- **Description**: [INSERT DESCRIPTION HERE]

### `void error(const char *message)`
- **Description**: [INSERT DESCRIPTION HERE]

### `void consumeHint(TokenType type, ErrorType errType, const char *message,
                 const char *hint)`
- **Description**: [INSERT DESCRIPTION HERE]

### `void consume(TokenType type, const char *message)`
- **Description**: [INSERT DESCRIPTION HERE]

### `void synchronize()`
- **Description**: [INSERT DESCRIPTION HERE]

### `void advance()`
- **Description**: [INSERT DESCRIPTION HERE]

### `bool check(TokenType type)`
- **Description**: [INSERT DESCRIPTION HERE]

### `bool checkTerminator(TokenType *terminators, int count)`
- **Description**: [INSERT DESCRIPTION HERE]

### `bool match(TokenType type)`
- **Description**: [INSERT DESCRIPTION HERE]

### `void ignoreNewlines()`
- **Description**: [INSERT DESCRIPTION HERE]

# Documentation: `parser.c`

## Overview
- **Purpose**: [INSERT MODULE PURPOSE HERE]

## Structs
### `ExpectedLabel`
- **Fields**: `uint32_t hash, int depth`
- **Description**: [INSERT DESCRIPTION HERE]

### `ParseRule`
- **Fields**: `PrefixFn prefix, InfixFn infix, Precedence precedence`
- **Description**: [INSERT DESCRIPTION HERE]

## Global/Static Variables
### `Parser parser;`
- **Description**: [INSERT DESCRIPTION HERE]

### `static ExpectedLabel expectedLabelStack[256];`
- **Description**: [INSERT DESCRIPTION HERE]

### `static int expectedLabelCount = 0;`
- **Description**: [INSERT DESCRIPTION HERE]

### `static int groupingDepth = 0;`
- **Description**: [INSERT DESCRIPTION HERE]

### `static int loopingDepth = 0;`
- **Description**: [INSERT DESCRIPTION HERE]

### `static int parseDepth = 0;`
- **Description**: [INSERT DESCRIPTION HERE]

### `ParseRule rules[] = {
    [TOKEN_AS] = {NULL, castExpression, PREC_CAST},
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {dict, instantiate, PREC_CALL}, // <--- Updated!
    [TOKEN_WITH] = {NULL, instantiateWith, PREC_CALL},   // <--- New!
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {explicitSticky, binary, PREC_FACTOR},
    [TOKEN_MOD] = {NULL, binary, PREC_FACTOR},
    [TOKEN_IS] = {stickyPrefix, binary, PREC_EQUALITY},
    [TOKEN_EQUAL_EQUAL] = {stickyPrefix, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {stickyPrefix, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {stickyPrefix, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {stickyPrefix, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {stickyPrefix, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {stickyPrefix, binary, PREC_COMPARISON},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_STRING_OPEN] = {interpolation, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_IT] = {implicitIt, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_UNLESS] = {NULL, NULL, PREC_NONE},
    [TOKEN_NOT] = {unary, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_TO] = {NULL, range, PREC_RANGE},
    [TOKEN_LEFT_BRACKET] = {list, subscript, PREC_CALL},
    [TOKEN_DOT] = {NULL, dot, PREC_CALL},
    [TOKEN_POSSESSIVE] = {NULL, possessive, PREC_CALL},
    [TOKEN_END] = {endKeyword, NULL, PREC_NONE},

    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};`
- **Description**: [INSERT DESCRIPTION HERE]

## Functions
### `static bool isExpectedLabel()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static char *my_strdup(const char *s)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static bool canStartExpression(TokenType type)`
- **Description**: [INSERT DESCRIPTION HERE]

### `bool isMathOperator(Token opToken)`
- **Description**: [INSERT DESCRIPTION HERE]

### `void errorAt(Token *token, ErrorType type, const char *message,
             const char *hint)`
- **Description**: [INSERT DESCRIPTION HERE]

### `void error(const char *message)`
- **Description**: [INSERT DESCRIPTION HERE]

### `void consumeHint(TokenType type, ErrorType errType, const char *message,
                 const char *hint)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static void consumeBlockEnd(Token opener, const char *blockName)`
- **Description**: [INSERT DESCRIPTION HERE]

### `void consume(TokenType type, const char *message)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static void consumeStatementEnd()`
- **Description**: [INSERT DESCRIPTION HERE]

### `void synchronize()`
- **Description**: [INSERT DESCRIPTION HERE]

### `void advance()`
- **Description**: [INSERT DESCRIPTION HERE]

### `bool check(TokenType type)`
- **Description**: [INSERT DESCRIPTION HERE]

### `bool match(TokenType type)`
- **Description**: [INSERT DESCRIPTION HERE]

### `bool checkTerminator(TokenType *terminators, int count)`
- **Description**: [INSERT DESCRIPTION HERE]

### `void ignoreNewlines()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static void validatePureExpression(Node *node, const char *context)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *parsePrecedence(Precedence precedence)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *expression()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *string()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *extractInterpolationString(Token token)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *interpolation()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *number()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *literal()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *implicitIt()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *variable()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *explicitSticky()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *unary()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *cloneNode(Node *original)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *stickyPrefix()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static bool isComparison(Token opToken)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *castExpression(Node *left)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *binary(Node *left)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *and_(Node *left)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *or_(Node *left)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *listComprehension(int line)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *dictComprehension(int line)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *list()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *dict()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *parseInstantiate(Node *left, bool isWith)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *instantiate(Node *left)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *instantiateWith(Node *left)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *subscript(Node *left)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *dot(Node *left)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *range(Node *left)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *possessive(Node *left)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *endKeyword()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *block(TokenType *terminators, int count)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *ifStatement(bool invert)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *whileLogic(bool invert)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *forStatement()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *parseLValue()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *addStatement()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *setStatement()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *giveStatement()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *expressionStatement()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *breakStatement()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *skipStatement()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *typeDeclaration()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Token makeHiddenToken(const char *text, int line)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *updateStatement()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *loadStatement()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *keepStatement()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *statement()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *parseTypeAnnotation()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *letDeclaration()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *grouping()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *propertySignatureDeclaration(int line)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *parsePropertySignatureBody(Token receiverName, Node *receiverType, int line)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static Node *declaration()`
- **Description**: [INSERT DESCRIPTION HERE]

### `static ParseRule *getRule(TokenType type)`
- **Description**: [INSERT DESCRIPTION HERE]

### `static void resetParserState()`
- **Description**: [INSERT DESCRIPTION HERE]

### `Node *parseSource(const char *source, int startLine)`
- **Description**: [INSERT DESCRIPTION HERE]

