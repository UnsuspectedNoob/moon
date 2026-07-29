# Documentation: `ast.h` & `ast.c` (Abstract Syntax Tree)

## Overview
- **Purpose**: The AST module defines the hierarchical tree structure that represents the syntactic structure of Moon source code. After the Scanner breaks the source code into Tokens, the Parser reads those Tokens and builds an Abstract Syntax Tree (AST). This tree is purely structural; it strips away formatting and semicolons, leaving only the logical relationships between operations and their operands. Moon uses a massive, unified `Node` struct with a highly optimized memory release mechanism.

## Structs (The Node Hierarchy)

### `NodeType` (enum)
- **Description**: An enumeration defining every possible type of node in the language. Examples include `NODE_LITERAL` (for numbers and strings), `NODE_BINARY` (for math and logic operations), `NODE_IF` (for conditionals), and `NODE_PHRASAL_CALL` (for invoking Moon's unique phrasal functions).

### `struct sNode` (The Master Node)
- **Fields**: `NodeType type`, `Node *parent`, `int line`, `bool usesIt`, `union as`
- **Description**: Moon does not use traditional C inheritance (like casting base structs to child structs). Instead, it uses a **Tagged Union**. Every single node in the tree is represented by this exact same struct. 
  - `type` identifies what kind of node it is.
  - `parent` is an invisible pointer pointing back up the tree, which is crucial for advanced traversal or error tracing.
  - `usesIt` is a powerful boolean flag used for implicit variable injection. If this node (or any of its children) uses the magic `it` keyword, this flag propagates all the way up the tree to automatically convert blocks into lambdas!
  - `union as` is the payload. It perfectly overlaps the memory of every possible payload struct (like `BinaryPayload` or `IfPayload`), ensuring a `Node` always takes exactly the same amount of memory regardless of what it represents, preventing heap fragmentation.

### Payload Structs (The Union Members)
- **Description**: These structs define the specific data each node needs to function.
  - **`BinaryPayload`**: Holds `left` (Node), `opToken` (Token), and `right` (Node).
  - **`PhrasalCallPayload`**: The complex engine behind Phrasal Functions. It holds the `mangledName` (e.g. `add$1_to$1`), the `arguments` array, and the `phraseTokens` array used to preserve the original English keywords for error reporting.
  - **`InterpolationPayload`**: Represents an interpolated string. It holds an array of `parts` which alternate between string literals and executable expression nodes.
  - **`ComprehensionPayload`**: Represents list/dict comprehensions (e.g., `[x * 2 for x in list]`). It tracks the `iterator`, `indexVar`, the `sequence` being iterated over, and the `body` expression.

## Functions

### `Node *allocateNode(NodeType type, int line)`
- **Description**: The internal base constructor. It allocates a single block of memory for a Node, zero-initializes it, sets the `type` and `line` number, and defaults the `parent` pointer to `NULL`.

### Node Constructors (`newBinaryNode`, `newIfNode`, `newPhrasalCallNode`, etc.)
- **Description**: A vast suite of constructor functions used by the Parser. Each constructor is responsible for allocating the base node, filling in its specific payload, and crucially, propagating the `usesIt` flag upwards. If any child node passed into the constructor has `usesIt == true`, the constructor will automatically set `usesIt = true` on the newly created parent node! It also automatically links the `parent` pointer of the children back to the new node.

### `void freeNode(Node *root)`
- **Description**: The AST Destructor. Since the AST can grow incredibly deep (e.g., a massive math equation or deeply nested blocks), recursively freeing it using C functions would cause a **Stack Overflow**. To prevent this, `freeNode` implements a brilliant **Iterative Depth-First Search (DFS)**. It creates a custom, heap-allocated stack (`NodeArray`), pushes the root node onto it, and processes the tree iteratively. For every node it pops off, it manually frees any dynamically allocated payload arrays (like a list of function arguments), pushes its children onto the stack, and finally frees the base node struct. This completely defuses the call stack bomb!
