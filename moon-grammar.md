# The Moon Grammar Specification

This document provides a comprehensive map of Moon's syntax and grammar, combining an EBNF-style structure with human-readable explanations to accommodate Moon's highly dynamic, contextual parsing capabilities.

## 1. Lexical Grammar (Tokens)

The foundational building blocks of Moon.

### 1.1 Keywords
Moon uses English-like keywords in place of traditional symbols for control flow and object manipulation.
```ebnf
keyword = "add" | "and" | "as" | "be" | "by" | "break" | "each"
        | "else" | "end" | "false" | "for" | "from" | "give"
        | "if" | "in" | "is" | "it" | "keep" | "let" | "load"
        | "nil" | "not" | "or" | "quit" | "set" | "skip"
        | "then" | "to" | "true" | "type" | "unless" | "until"
        | "update" | "while" | "with" ;
```

### 1.2 Identifiers
Used for variable names, property names, and method signatures.
```ebnf
identifier = letter , { letter | digit } ;
letter     = "a" ... "z" | "A" ... "Z" | "_" ;
digit      = "0" ... "9" ;
```

### 1.3 Numbers
Moon supports standard decimal numbers, hexadecimals, binaries, and fractional/floating-point values.
```ebnf
number   = decimal | hex | binary ;
decimal  = digit , { digit } , [ "." , digit , { digit } ] ;
hex      = "0" , ("x" | "X") , hexDigit , { hexDigit } ;
binary   = "0" , ("b" | "B") , binaryDigit , { binaryDigit } ;

hexDigit = digit | "a"..."f" | "A"..."F" ;
binaryDigit = "0" | "1" ;
```

### 1.4 Strings & Interpolation
Moon uses double quotes `"` for strings and backticks ``` ` ``` to enter and exit interpolation state.

```ebnf
string          = '"' , { character } , '"' ;
interpolated    = '"' , { character } , "`" , expression , "`" , { character } , '"' ;
```
> [!NOTE] 
> Interpolation can be deeply nested. When the scanner hits a backtick ``` ` ```, it temporarily yields back to the parser to evaluate the inner expression, before resuming string parsing at the next backtick.

### 1.5 Symbols & Operators
```ebnf
symbol = "(" | ")" | "{" | "}" | "[" | "]" | "," | ":" | "." 
       | "-" | "%" | "+" | "/" | "*" | "!" | "!=" | "=" | "==" 
       | ">" | ">=" | "<" | "<=" | "'s" ;
```
*Note: Moon has no statement terminator symbol (like `;`). Instead, newlines `\n` and control block boundaries (`end`, `else`, `then`) act as statement boundaries.*

## 2. Program Structure & Declarations

A Moon program is a sequence of declarations.

```ebnf
program = { declaration } , EOF ;

declaration = typeDeclaration 
            | letDeclaration 
            | propertySignatureDeclaration
            | statement ;
```

### 2.1 Types (Blueprints)
Types define the structure and default values of objects.
```ebnf
typeDeclaration = "type" , identifier , [ "is" , typeName ] , ":" , [ properties ] , "end" ;
properties      = property , { "," , property } ;
property        = identifier , ":" , expression ;
```

### 2.2 Variables
```ebnf
letDeclaration = "let" , identifier , "be" , expression ;
```

### 2.3 Functions & Phrasal Methods
Functions are declared using `let` and can interleave parameters to create phrasal signatures.
```ebnf
functionDeclaration = "let" , identifier , { functionSegment } , ":" , block , "end" ;
functionSegment     = "(" , identifier , [ ":" , typeAnnotation ] , ")" 
                    | identifier | "'s" ;
typeAnnotation      = identifier , { "or" , identifier } ;
```
> [!NOTE] 
> Phrasal methods interleave parameter names and labels. For example: `let describe (entity: Player or Enemy)'s status:`. The label words become the method signature.

### 2.4 Extension Methods & Blueprint Methods
Methods can be directly embedded in Blueprints using `my`, or attached externally using `let (receiver)`.
```ebnf
embeddedMethod = "my" , identifier , { functionSegment } , ":" , block , "end" ;

extensionMethod = "let" , "(" , identifier , [ ":" , typeAnnotation ] , ")" , "'s" , 
                  identifier , { functionSegment } , ":" , block , "end" ;
```

## 3. Statements & Control Flow

```ebnf
statement = ifStatement | whileStatement | forStatement 
          | giveStatement | setStatement | addStatement 
          | updateStatement | keepStatement | loadStatement 
          | "break" | "skip" | "quit" 
          | expressionStatement ;
```

### 3.1 Blocks
A block is a sequence of declarations bounded by specific terminators (usually `end`, but can be `else` or `then`).
```ebnf
block = { declaration } ;
```

### 3.2 Conditional Logic (If / Unless)
Moon supports both multi-line blocks and single-line conditional branches.
```ebnf
ifStatement     = ( "if" | "unless" ) , expression , ( blockBranch | singleBranch ) ;

blockBranch     = ":" , block , [ alternateBlock ] , "end" ;
alternateBlock  = ( "else" | "then" ) , ( "if" | "unless" ) , ifStatement
                | ( "else" | "then" ) , ":" , block ;

singleBranch    = statement , [ ( "else" | "then" ) , statement ] ;
```

### 3.3 Loops (While / Until / For)
```ebnf
whileStatement  = ( "while" | "until" ) , expression , ( ":" , block , "end" | statement ) ;

forStatement    = "for" , [ "each" ] , identifier , [ "," , identifier ] , 
                  ( "in" | "from" ) , expression , 
                  ( ":" , block , "end" | statement ) ;
```

### 3.4 Data Actions
Moon provides expressive data manipulation actions. Most data actions operate on an `lvalue` (a resolvable memory address).
```ebnf
lvalue       = lvalueBase , { lvalueSuffix } ;
lvalueBase   = ( "my" , identifier ) | identifier ;
lvalueSuffix = "[" , expression , "]" 
             | "." , identifier 
             | "'s" , identifier ;
```

```ebnf
setStatement    = "set" , lvalue , ( "=" | "to" ) , expression ;
addStatement    = "add" , expression , "to" , lvalue ;
updateStatement = "update" , lvalue , ( ( "+" | "-" | "*" | "/" | "%" ) , expression | "as" , expression ) ;
keepStatement   = "keep" , identifier , ( "=" | "as" | "be" ) , expression ;
loadStatement   = "load" , string ;
giveStatement   = "give" , [ expression ] ;
```
### 3.5 Statement Modifiers
Any single-line statement can be modified with a trailing conditional.
```ebnf
statementModifier = statement , ( "if" | "unless" ) , expression ;
```
*Note: `lvalue` refers to a variable or property accessor (`a.b.c`).*

## 4. Expressions & Precedence

Moon expressions are parsed using a Pratt Parser, which means precedence strictly dictates how operators bind.

```ebnf
expression = assignmentExpr ;
```

### 4.1 Operator Precedence (Lowest to Highest)
1. **Range** (`to`, `by`)
2. **Logic OR** (`or`)
3. **Logic AND** (`and`)
4. **Equality** (`is`, `is not`, `==`, `!=`, `=`)
5. **Comparison** (`>`, `>=`, `<`, `<=`)
6. **Term** (`+`, `-`)
7. **Factor** (`*`, `/`, `%`)
8. **Cast** (`as`)
9. **Unary** (`not`, `!`, `-`)
10. **Call / Property** (`()`, `{}`, `[]`, `.`, `'s`, Phrasal method chains)
11. **Primary** (Literals, Identifiers, Grouping `()`)
*Note: Assignment is handled strictly as a Statement (`set x to 10`, `update x + 5`), so there is no expression-level assignment precedence!*

### 4.2 Mathematical & Logical Operations
```ebnf
binaryExpr = expression , operator , expression ;
operator   = "and" | "or" | "is" | "is not" | "==" | "!=" | "=" | ">" | ">=" 
           | "<" | "<=" | "+" | "-" | "*" | "/" | "%" ;

unaryExpr  = ( "not" | "!" | "-" ) , expression ;
```

### 4.3 Chained Comparisons
Moon allows mathematical comparison chains directly in the syntax without needing explicit `and` statements.
```ebnf
chainedComparison = expression , comparisonOperator , expression , { comparisonOperator , expression } ;
comparisonOperator = "is" | "is not" | "==" | "!=" | "=" | ">" | ">=" | "<" | "<=" ;
```
*Example: `1 < x <= 10` is parsed as a single continuous chain!*

### 4.4 Sticky Subjects (Implicit Prefix Comparisons)
In Moon, comparison operators can be used dynamically as **Prefix Operators**. When an expression begins with a comparison operator, it implicitly binds to the current subject/iterator in context.
```ebnf
stickyPrefix = comparisonOperator , expression ;
```
*Example: `until is 10:` or `let evens be [for each n in nums keep n if < 10]`*

### 4.5 Phrasal Method Calls (The Signature Trie)
Moon's most unique feature. It dynamically absorbs identifiers to form method signatures until no further matching path exists in the Trie.

```ebnf
phrasalCall = expression , { identifier , [ expression ] } ;
```
> [!TIP]
> **How it works:** If you write `player attack enemy with sword`, the parser reads `player` as the primary expression. Then it sees `attack`, checks if `attack` is a method on `player`. It finds `attack_with`, so it consumes `enemy` as an argument, then consumes `with`, then consumes `sword` as the final argument.

## 5. Collections & Data Types

### 5.1 Lists & List Comprehensions
```ebnf
list              = "[" , [ expression , { "," , expression } ] , "]" ;

listComprehension = "[" , "for" , [ "each" ] , identifier , [ "," , identifier ] , 
                    ( "in" | "from" ) , expression , 
                    ( ":" , block , "end" | statement ) , "]" ;
```

### 5.2 Dictionaries & Instantiation
Dictionaries are JSON-like. If a dictionary immediately follows a Type identifier or variable, it triggers *Instantiation*.
```ebnf
dictionary    = "{" , [ keyValuePair , { "," , keyValuePair } ] , "}" ;
keyValuePair  = ( identifier | string ) , ":" , expression ;

instantiation = expression , [ "with" ] , dictionary ;
```
*Example: `Player { name: "Emrys" }` or `player with { hp: 10 }`*

### 5.3 Ranges & Slicing
Ranges generate numbers, slicing extracts them.
```ebnf
range = "[" , expression , "to" , expression , [ "by" , expression ] , "]" ;

slice = expression , "[" , expression , [ "to" , expression , [ "by" , expression ] ] , "]" ;
```
