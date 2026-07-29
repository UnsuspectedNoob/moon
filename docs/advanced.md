# Documentation: Advanced Features (`sigtrie` & `subscript`)

## Overview
- **Purpose**: This module covers two of the most complex runtime features in the Moon language: 
  - `sigtrie.h/c` implements the **Signature Trie**, a sophisticated data structure used to rapidly map Moon's natural-language "Phrasal Functions" (e.g., `add (x) to (y)`) into their internal mangled names (`add$1_to$1`).
  - `subscript.h/c` provides the heavy-lifting logic for dynamic indexing and slicing of Lists, Dictionaries, and Strings (e.g., `list[1 to 5 by 2]`).

---

## 1. `sigtrie.h` & `sigtrie.c` (The Signature Trie)

### Struct: `RegistryEntry`
- **Fields**: `uint32_t rootHash`, `char *rootWord`, `TrieNode *trieRoot`
- **Description**: Represents a bucket in the global hash tables (`phrasalTable` and `propertyTable`). The `rootHash` is cached to speed up string comparisons during the linear probing sequence. 

### Struct: `TrieNode`
- **Fields**: `PhraseNodeType type`, `uint32_t labelHash`, `char *labelName`, `int arity`, `bool isTerminal`, `bool isCore`, `char *mangledName`, `TrieNode **children`, `int childCount`, `int childCapacity`
- **Description**: A single node in the Trie. 
  - `type` dictates if the node is a `NODE_LABEL` (a specific keyword like "to") or a `NODE_ARGUMENT` (a variable slot).
  - `isTerminal` flags if this node marks the end of a valid phrase. If true, `mangledName` holds the compiled C-string name used by the VM.
  - `isCore` flags if this node was created during `bootstrapCore()`. This allows user-defined phrases to be wiped between REPL inputs without destroying the standard library!

### Hash Tables
#### `static RegistryEntry phrasalTable[TABLE_CAPACITY];` & `static RegistryEntry propertyTable[TABLE_CAPACITY];`
- **Description**: Two massive, statically allocated Hash Tables (capacity 1024) that use **Open Addressing with Linear Probing** to resolve collisions. They store the "Root Word" of every phrase (e.g., the `add` in `add (x) to (y)`).

### Core Functions
#### `TrieNode *getSignatureTrie(const char *rootWord)`
- **Description**: Executes the O(1) hash lookup with linear probing to find the root `TrieNode` for a given starting word.

#### `TrieNode *startPhrase(const char *rootWord, int length)`
- **Description**: Searches the hash table for the root word. If it doesn't exist, it allocates a new `RegistryEntry`, initializes a new `NODE_LABEL` TrieNode, and inserts it into the table.

#### `TrieNode *addLabelBranch(...)` & `TrieNode *addArgumentBranch(...)`
- **Description**: Tree builders. They linearly scan a node's `children` array to see if the branch (e.g., the keyword "to" or an argument of arity `1`) already exists. If not, they dynamically allocate a new `TrieNode` and append it to the dynamically resizing `children` array.

#### `void registerSignature(const char *root, const char *path, const char *mangledName)`
- **Description**: The master compiler function used during initialization. It takes a raw path string (like `$,to,$`), tokenizes it using C's `strtok()`, and iteratively walks down the Trie, calling `addArgumentBranch()` for `$` tokens and `addLabelBranch()` for text tokens, finally calling `finalizePhrase()` on the leaf node.

#### `void freeSignatureTable()` & `static void freeTrieNode(TrieNode *node)`
- **Description**: A highly intelligent destructor. When called, it recursively walks the Trie and frees all memory. Crucially, it checks the `isCore` flag. It will completely bypass and preserve any branches belonging to the core standard library while mercilessly destroying user-defined branches!

---

## 2. `subscript.h` & `subscript.c` (Dynamic Indexing & Slicing)

### Core Functions
#### `bool executeGetSubscript(Value seqVal, Value indexVal, Value *result)`
- **Description**: The massive routing function called by the `OP_GET_SUBSCRIPT` bytecode instruction. It inspects the `seqVal` type at runtime and routes it to the correct extraction logic:
  - **Lists**: Allows both single Number access and Range slicing. It enforces **1-based indexing** (converting user index 1 to C index 0). It also fully supports **Negative Indexing** (e.g., `-1` becomes `list->count - 1`)! If a `Range` is passed (e.g. `1 to 10 by 2`), it dynamically allocates a *new* List and uses a step-aware `for` loop to slice the original array.
  - **Strings**: Identical logic to Lists, but extracts `char`s. When single-indexed, it returns a 1-character String Object. When sliced, it allocates a new C-string buffer, populates it, and hands it off to `takeString()`.
  - **Dictionaries**: Bypasses indexing entirely and directly calls `tableGet()` to hash the `indexVal` as a key. If missing, it safely returns `NIL_VAL`.

#### `bool executeSetSubscript(Value collectionVal, Value indexVal, Value value, Value *result)`
- **Description**: The routing function for `OP_SET_SUBSCRIPT`. 
  - **Dictionaries**: Calls `tableSet()` to insert or overwrite the key-value pair.
  - **Lists**: Calculates the 1-based (and potentially negative) index and directly overwrites the pointer in the `list->items` array. Note: Unlike Python, Moon does *not* support replacing slices with other slices!
  - **Strings**: Strings in Moon are immutable! If a user tries to assign to a string index, this function intentionally falls through to a `runtimeErrorDetailed()`!
