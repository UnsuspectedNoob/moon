# Detailed Analysis of `static Node *variable()`

The `static Node *variable()` function in `parser.c` is responsible for parsing identifiers in your language. Because this language supports both standard variables and multi-word "phrasal function calls" (e.g., `add 5 to 10`), this function is highly sophisticated. It essentially acts as a Deterministic Finite Automaton (DFA) engine that walks a Trie data structure to match multi-word syntax.

Below is a thorough explanation of every structure, function, and logical step you will encounter in this function.

## 1. Core Structures Explained

*   **`Token`** (`scanner.h`): Represents a single chunk of scanned text. It contains the token's `type` (e.g., `TOKEN_IDENTIFIER`), a `start` pointer to the original source string, its `length`, and `line` information.
*   **`TokenType`** (`scanner.h`): An enum representing all valid tokens, like `TOKEN_NEWLINE`, `TOKEN_LET`, `TOKEN_COMMA`, etc.
*   **`Node`** (`ast.h`): The central Abstract Syntax Tree (AST) structure. It contains a `NodeType` (e.g., `NODE_VARIABLE`, `NODE_PHRASAL_CALL`), positional info, and a massive `union` (like `as.variable` or `as.phrasalCall`) that holds the specific payload for that node type.
*   **`NodeArray`** (`memory.h` / `ast.h`): A dynamic array structure holding `Node *` pointers. It is generated via the macro `DECLARE_ARRAY(Node *, NodeArray)`. It contains an array of `items`, a `count`, and a `capacity`.
*   **`TrieNode`** (`sigtrie.h`): A node in a DFA representing a piece of a phrasal function's signature. 
    *   It has a `PhraseNodeType` (`NODE_LABEL` for literal words like "to", or `NODE_ARGUMENT` for expressions). 
    *   It contains an array of `children` (possible next words/arguments in the phrase). 
    *   It has an `isTerminal` flag to indicate if stopping here represents a valid, complete function call, and a `mangledName` (the backend C-name).

## 2. Core Functions & Globals Explained

*   **`parser`**: A global struct maintaining the parser's state, most notably `parser.previous` (the token we just consumed) and `parser.current` (the token we are currently looking at).
*   **`advance()`**: Consumes `parser.current`, moving it into `parser.previous`, and scans the next token into `parser.current`.
*   **`check(TokenType type)`**: Returns `true` if `parser.current.type == type`.
*   **`getSignatureTrie(const char *rootWord)`**: Takes the first word of an identifier and returns the root `TrieNode` if a phrasal function starting with this word exists. Returns `NULL` otherwise.
*   **`hashString(const char *key, int length)`**: Hashes a string (like the current token) into a `uint32_t` so it can be quickly compared against `TrieNode->labelHash`.
*   **`canStartExpression(TokenType type)`**: Returns `true` if the given token could validly start an expression (e.g., returning `false` for `TOKEN_RIGHT_PAREN` or `TOKEN_ELSE`).
*   **`expression()`**: A recursive descent (Pratt) parser function that parses an entire expression and returns an AST `Node*`.
*   **`validatePureExpression(Node *node, const char *context)`**: Ensures an expression doesn't illegally contain statement-level modifiers (like a ternary `if` missing its `else` branch).
*   **`errorAt()`**: Generates a formatted syntax error at a specific token.
*   **`my_strdup()`**: A helper to duplicate a C-string on the heap.
*   **AST Node Constructors (`ast.h`)**:
    *   `newVariableNode(Token name, int line)`: Creates a basic variable node.
    *   `newPropertyNode(Node *target, Token name, int line)`: Creates a property access node.
    *   `newPhrasalCallNode(Token mangledName, Node **args, int argCount, Token *phraseTokens, int phraseTokenCount, int line)`: Creates a node representing a completed phrasal function call.

## 3. Step-by-Step Walkthrough of `variable()`

### Step 1: The Root Word & Quick Fallback
The function starts assuming `parser.previous` is an identifier (the root token). It extracts this word into a buffer (`rootWord`) and passes it to `getSignatureTrie()`.

If `getSignatureTrie()` returns `NULL`, it means there are no multi-word functions starting with this word. The function then does a quick check: is the word literally `"my"`? If so, and the *next* token is an identifier, it parses it as a property access (`newPropertyNode`), giving you `my.property` semantics. If not, it simply returns a standard variable node (`newVariableNode`).

### Step 2: Setting up the DFA Engine
If a signature trie *was* found, we might be looking at a multi-word function. 
*   It initializes a `NodeArray args` to accumulate arguments.
*   It sets `currentNode` to the start of the Trie.
*   It tracks `lastGoodState` (the last `isTerminal` node we visited), so if the phrase abruptly stops, we can fallback to the last valid interpretation.
*   It tracks an array of `phraseTokens` to remember exactly which tokens made up the phrase for debugging.

### Step 3: The Traversal Loop
It enters a `while (currentNode->childCount > 0)` loop. It hashes `parser.current` and iterates over all children of the `currentNode` to see what moves are legal next:
*   **Looking for Labels**: It checks if any child is a `NODE_LABEL` that matches the hash of the current token.
*   **Looking for Arguments**: It checks if any child is a `NODE_ARGUMENT`.

### Step 4: Matching a Label Branch
If it found a matching label (e.g., the word "to" in `add 5 to 10`), it adds the token to `phraseTokens`, calls `advance()` to consume the word, moves `currentNode` to that child, and updates `lastGoodState` if the new node is terminal. It then loops again `continue;`.

### Step 5: Matching an Argument Branch
If no label matched, but the DFA expects an argument, and `canStartExpression(parser.current.type)` is true, it parses the argument:
1.  **Label Prediction**: It looks ahead at the DFA children to see what labels follow this argument. It pushes these label hashes to a global `expectedLabelStack`. This tells the `expression()` parser to *stop* parsing if it sees one of these words, preventing the expression parser from eating words that belong to the phrase signature!
2.  **Parsing**: It calls `expression()` to parse the argument.
3.  **Unpacking**: It unwraps tuple or grouping nodes and stores the resulting nodes in a temporary array (`tempArgs`).
4.  **Arity Matching**: It checks the DFA children again to find the `NODE_ARGUMENT` branch whose `arity` matches the number of arguments we just parsed.
5.  **Commit or Bail**: If it finds a matching arity, it transfers `tempArgs` into the main `args` array, moves `currentNode` forward, and updates `lastGoodState`. If no arity matches, it's a dead end—it frees the temporary arguments and breaks the loop.

### Step 6: Finalizing the Parse
When the loop ends, the function checks if we ended on a valid state:
*   If we did not land on `lastGoodState`, it means the phrase was incomplete (e.g., `add 5...` but missing `to 10`). It emits a syntax error, frees the arguments, and returns a plain variable node as a safe fallback.
*   If we successfully matched a phrase, it constructs a fake `Token mangledToken` whose string is the `mangledName` from the terminal Trie node.
*   It validates all arguments (except the final one) using `validatePureExpression` to ensure they don't contain illegal statement modifiers.
*   Finally, it packages the mangled name, the collected arguments, and the tokens into a `newPhrasalCallNode()` and returns it.
