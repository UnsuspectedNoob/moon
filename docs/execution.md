# Documentation: Execution Pipeline (`vm`, `chunk`, & `memory`)

## Overview
- **Purpose**: This trio of modules forms the Runtime Engine of the Moon compiler. After the compiler has lowered the AST into bytecode, the Virtual Machine (VM) takes over to execute those bytes at blazing fast speeds.
  - `chunk.h/c` defines the bytecode instruction set and the data structures that hold the compiled code.
  - `memory.h/c` manages all dynamic memory allocation and contains the Mark-and-Sweep Garbage Collector (GC).
  - `vm.h/c` is the core execution loop, managing the call stack, global variables, and interpreting opcodes.

---

## 1. `chunk.h` & `chunk.c` (The Instruction Set Architecture)

### Struct: `Chunk`
- **Fields**: `int count`, `int capacity`, `uint8_t *code`, `int *lines`, `ValueArray constants`, `InlineCacheArray caches`
- **Description**: A `Chunk` is a sequence of bytecode. 
  - `code` is a dynamically growing array of raw bytes containing the actual executable instructions (`OpCode`s).
  - `lines` runs parallel to `code` and tracks the original source code line number for every byte to provide accurate stack traces.
  - `constants` is a pool of `Value`s used in the code.
  - `caches` is an optimization structure used to speed up Phrasal Function multiple-dispatch lookups.

### Struct: `InlineCacheEntry` & `InlineCacheArray`
- **Description**: These structures support Moon's multiple dispatch system. When a phrasal function is called, the VM caches the exact types of the arguments passed. The next time the function is called, if the argument types match the `InlineCacheEntry`, the VM completely skips the expensive signature lookup!

### Functions
#### `void initChunk(Chunk *chunk)` & `void freeChunk(Chunk *chunk)`
- **Description**: Lifecycle functions. `initChunk` zeroes out all fields, and `freeChunk` uses the memory module's `FREE_ARRAY` macros to deallocate the arrays.

#### `void writeChunk(Chunk *chunk, uint8_t byte, int line)`
- **Description**: Appends a byte of bytecode (an OpCode or operand) to the chunk's `code` array, dynamically growing it if capacity is reached. Also records the line number.

#### `int addConstant(Chunk *chunk, Value value)`
- **Description**: Appends a constant to the `constants` ValueArray and returns its index. 

#### `int addCacheEntry(Chunk *chunk)`
- **Description**: Appends an empty cache slot to the `caches` array and returns its index, to be populated later by the VM during runtime execution.

---

## 2. `vm.h` & `vm.c` (The Virtual Machine)

### Struct: `CallFrame`
- **Fields**: `ObjFunction *function`, `uint8_t *ip`, `Value *slots`, `Table *globals`, `Value stickySubject`
- **Description**: Represents a single active function call. 
  - `ip` (Instruction Pointer) points directly into the function's bytecode chunk.
  - `slots` points to the section of the VM's massive data stack reserved for this function's local variables.
  - `stickySubject` holds the state for Moon's unique chained comparisons (e.g., `if x == 5 or == 10`).

### Struct: `VM`
- **Fields**: `CallFrame frames[FRAMES_MAX]`, `Value stack[STACK_MAX]`, `Value *stackTop`, `Obj *objects`, `size_t bytesAllocated`
- **Description**: The global singleton state. It holds the Call Stack (`frames`) and the Data Stack (`stack`). It also maintains pointers to all heap-allocated objects (`objects`) so the GC can trace them, and tracks `bytesAllocated` to trigger GC sweeps.

### Global Functions
#### `void initVM()` & `void freeVM()`
- **Description**: `initVM` zeroes the stack pointers, initializes global hash tables (like the string intern pool), and prepares the GC state. `freeVM` calls `freeObjects()` to destroy the entire heap.

#### `void bootstrapCore()`
- **Description**: An essential initialization step that injects all the native C functions (like standard math and string libraries) into the global scope before user code runs.

#### `InterpretResult interpret(const char *source, int startLine)`
- **Description**: The highest-level entry point. It calls `compile()`, loads the resulting script function into a `CallFrame`, and triggers `run()`.

#### `static InterpretResult run()`
- **Description**: The hottest loop in the compiler. It does NOT use a standard C `switch` statement! Instead, it uses **Computed Gotos** (`goto *dispatchTable[instruction]`) combined with a `DISPATCH()` macro. This advanced optimization skips the overhead of a switch loop entirely, jumping directly to the C logic for each opcode (like `TARGET_OP_ADD`), executing it, and jumping straight to the next!

#### Stack Operations: `void push(Value value)`, `Value pop()`, `Value peek(int distance)`
- **Description**: Highly optimized macros/functions for interacting with the VM's data stack. 

#### `bool isFalsey(Value value)` & `bool valuesEqual(Value a, Value b)`
- **Description**: Centralized logic for type comparison. `isFalsey` dictates that only `nil` and `false` are considered false.

#### Static Linker API: `void registerNativePhrasal(...)`, `void defineNative(...)`
- **Description**: Exposes C functions to Moon by wrapping them in `ObjNative` or `ObjMultiFunction` structs and injecting them into the global variables table.

#### Error Handling: `void runtimeErrorDetailed(...)`, `void throwNativeError(...)`
- **Description**: Halts execution safely. It walks backward down the `frames` stack to print a beautiful, multi-layered stack trace, utilizing the `lines` array stored in the Chunks!

---

## 3. `memory.h` & `memory.c` (The Garbage Collector)

### Macros (`ALLOCATE`, `FREE`, `GROW_ARRAY`)
- **Description**: Wrappers around `reallocate()`. By routing all allocations through a single point, the VM accurately tracks `bytesAllocated`.

### Memory Functions
#### `void *reallocate(void *pointer, size_t oldSize, size_t newSize)`
- **Description**: The memory controller. If `newSize` is larger than `oldSize`, it allocates more memory. Crucially, if total `bytesAllocated` exceeds the `nextGC` threshold, this function immediately suspends execution and triggers `collectGarbage()` before allocating!

#### `void freeObject(Obj *object)` & `void freeObjects()`
- **Description**: Handles the teardown of specific objects based on their `ObjType` (e.g. freeing the internal character array of a string). `freeObjects` sweeps the entire heap on VM shutdown.

### Garbage Collection API
#### `void collectGarbage()`
- **Description**: The master trigger for the GC cycle. It calls `markRoots()`, then `traceReferences()`, and finally `sweep()`.

#### `static void markRoots()`
- **Description**: Sweeps through all intrinsically accessible values: variables on the data stack, global variables, and constants in active CallFrames, passing them to `markValue()`.

#### `void markValue(Value value)` & `void markObject(Obj *object)`
- **Description**: Checks if the value is a heap object. If it is, and its `isMarked` flag is false, it flips it to true. Then, it pushes the object onto the VM's `grayStack` to be traced later.

#### `static void traceReferences()` & `static void blackenObject(Obj *object)`
- **Description**: The iterative tracer. It pops objects off the `grayStack` and "blackens" them by calling `markValue` on any internal references they hold (e.g., if it's a List, it marks all values inside the list). Using the `grayStack` completely eliminates recursive C stack overflows!

#### `static void sweep()`
- **Description**: Walks the VM's massive linked list of *all* allocated objects (`vm.objects`). If an object is NOT marked (meaning `traceReferences` never reached it), it is completely inaccessible and immediately passed to `freeObject()`! It then resets `isMarked` for the survivors.
