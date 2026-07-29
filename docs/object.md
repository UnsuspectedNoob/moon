# Documentation: `object.h` & `object.c` (Heap Allocation Module)

## Overview
- **Purpose**: While the `value.h` module handles primitives that fit inside a 64-bit value, the `object.h` module handles dynamically allocated, complex data structures that live on the Heap (like Strings, Lists, Dictionaries, and Functions). Every heap-allocated structure in Moon "inherits" from a base `Obj` struct. This allows the Garbage Collector (GC) to treat all complex objects uniformly when tracing and sweeping memory.

## Structs (The Object Hierarchy)

### `Obj` (The Base Class)
- **Fields**: `ObjKind type`, `bool isMarked`, `struct Obj *next`
- **Description**: The fundamental header struct that sits at the very beginning of *every* heap object in memory. 
  - `type` identifies what kind of object it is (String, List, etc.).
  - `isMarked` is used by the mark-and-sweep Garbage Collector to track if the object is still reachable.
  - `next` is an intrusive linked-list pointer. Every allocated object in Moon is strung together into one massive linked list managed by the VM!

### `ObjString`
- **Fields**: `Obj obj`, `int length`, `char *chars`, `uint32_t hash`, `struct ObjString *left`, `struct ObjString *right`
- **Description**: Represents a string of text. To achieve incredible performance during string concatenation, Moon strings can act as a **Rope Data Structure**. If a string is created by joining two others together, it simply points to them (`left` and `right`) without allocating new memory for the characters! It is only "flattened" into a contiguous array of characters (`chars`) when strictly necessary. Strings are also heavily interned in a global Hash Table to save memory and allow strings to be compared for equality by simply comparing their memory addresses!

### `ObjFunction`
- **Fields**: `Obj obj`, `int arity`, `Chunk chunk`, `ObjString *name`, `ObjModule *module`, `Table *homeGlobals`, `bool isTopLevel`
- **Description**: The compiled representation of a user-defined function or a script. It holds the actual bytecode `chunk` that the Virtual Machine will execute, alongside metadata about how many arguments it expects (`arity`). It also keeps a pointer back to its `module` and `homeGlobals` to ensure it can always access its original environment variables, no matter where it is executed!

### `ObjMultiFunction`
- **Fields**: `Obj obj`, `ObjString *name`, `int arity`, `int methodCount`, `int methodCapacity`, `Value *methods`, `Value **signatures`
- **Description**: The engine behind Moon's powerful **Multiple Dispatch** feature. When you declare multiple versions of the same Phrasal Function (e.g. `add (x: Number)` and `add (x: String)`), Moon wraps them all up in an `ObjMultiFunction`. When the phrase is called, the VM intercepts this object, looks at the runtime types of the arguments provided, cross-references them against the `signatures` table, and executes the correct corresponding bytecode chunk in the `methods` array!

### `ObjList`
- **Fields**: `Obj obj`, `int count`, `int capacity`, `Value *items`
- **Description**: A dynamically growing contiguous array (vector) for ordered collections. It works identically to `ValueArray` but is explicitly allocated on the heap as an object so it can be passed by reference and managed by the GC.

### `ObjDict`
- **Fields**: `Obj obj`, `Table fields`
- **Description**: A collection of key-value pairs. It directly wraps the highly optimized `Table` struct (used elsewhere in the compiler for variables) to provide instant Hash Map lookups.

### `ObjType` & `ObjInstance`
- **Fields**: `Obj obj`, `ObjString *name`, `Table properties`, `bool isNative` (ObjType)
- **Fields**: `Obj obj`, `ObjType *type`, `Table fields` (ObjInstance)
- **Description**: Moon's Custom Data Blueprint system. 
  - `ObjType` acts as the template or blueprint. It holds the name and any static properties or methods.
  - `ObjInstance` is the actual physical clone living in memory. It holds its own unique data in its `fields` dictionary, and keeps an invisible pointer back to its parent `ObjType` to figure out what methods it can run!

### `ObjUnion`
- **Fields**: `Obj obj`, `int count`, `Value *types`
- **Description**: Represents a type signature that accepts multiple different blueprints (e.g., `Number or String`). When generating signatures for `ObjMultiFunction`s, unions are evaluated to ensure all possible paths are covered.

### `ObjModule`
- **Fields**: `Obj obj`, `ObjString *name`, `ObjString *source`, `Table fields`
- **Description**: Represents an imported file or distinct execution space. It encapsulates its own global variables inside the `fields` table to prevent variable name collisions between different files!

## Core Functions

### `uint32_t hashString(const char *key, int length)`
- **Description**: Implements the FNV-1a Hash Algorithm. It generates an extremely uniform 32-bit hash for a given string of text, which powers all dictionary lookups and variable resolutions in Moon.

### `void flattenString(ObjString *string)`
- **Description**: The core algorithm for Moon's String Ropes. It performs an iterative Depth-First Search (DFS) over the binary tree of nested strings, allocating a single contiguous block of memory, copying all leaf characters into it, and completely severing the tree branches to leave a perfectly flat, cache-friendly string.

### `void stringifyValueToBuffer(Value value, int indent, StringBuffer* sb)`
- **Description**: The engine behind the `show` command. It takes any Moon value, inspects its type, and meticulously formats it into a human-readable text string. For complex collections like `ObjDict`, it recursively calls itself to seamlessly pretty-print deeply nested structures!

### Object Allocators (`newFunction`, `newList`, `newDict`, `newType`, `newInstance`, etc.)
- **Description**: A suite of constructor functions. Every single one of these internally calls `allocateObject()`, which asks the GC for memory, stamps the base `Obj` header, and registers the new object with the VM's linked list to guarantee it will eventually be swept away when no longer needed.
