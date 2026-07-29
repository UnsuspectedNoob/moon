# Documentation: `value.h` & `value.c` (Core Value System)

## Overview
- **Purpose**: This module defines how the Moon programming language represents all its core data types (Primitives and Pointers) at the lowest memory level. To maximize performance and memory efficiency, Moon uses a technique called "NaN Boxing". This technique packs a floating-point number, boolean, `nil`, or an object pointer perfectly inside a single 64-bit `uint64_t` value!

## Structs

### `ValueArray`
- **Fields**: `int capacity, int count, Value *values`
- **Description**: A dynamic array (vector) for storing `Value`s. It automatically resizes its `values` array by doubling its capacity whenever it runs out of space. This is used extensively by the compiler to hold constants and by list objects to hold elements.

## Data Types & NaN Boxing

### `typedef uint64_t Value;`
- **Description**: The absolute core of Moon's memory model. Everything in Moon is represented as a 64-bit `Value`. 

### The Masks (`SIGN_BIT`, `QNAN`, `TAG_*`)
- **Description**: In the IEEE 754 specification for 64-bit doubles, there is a massive range of bits reserved for "Not a Number" (NaN). Moon "steals" these unused bits! 
  - If a 64-bit value is a valid double, it's just a regular Number.
  - If the `QNAN` bits are set, it's a special Moon value.
  - The lowest bits of the mantissa (`TAG_NIL`, `TAG_FALSE`, `TAG_TRUE`) indicate exactly which primitive it is.
  - If the `SIGN_BIT` is also set, it means the lowest 48 bits actually hold a memory address pointing to a heap-allocated `Obj` (like a String or List).

## Functions

### `static inline double valueToNum(Value value)` & `static inline Value numToValue(double num)`
- **Description**: Uses a C `union` for "Type Punning" to safely trick the C compiler into converting between a raw 64-bit unsigned integer and a 64-bit floating point double without accidentally corrupting the bits or invoking undefined behavior.

### `void initValueArray(ValueArray *array)`
- **Description**: Initializes an empty dynamic array, setting its capacity and count to 0.

### `void writeValueArray(ValueArray *array, Value value)`
- **Description**: Appends a `Value` to the dynamic array. If the array has run out of space, it allocates a larger chunk of memory and moves the existing items over.

### `void freeValueArray(ValueArray *array)`
- **Description**: Frees the memory block managed by the array.

### `uint32_t hashValue(Value value)`
- **Description**: Generates a robust 32-bit hash for any `Value`. For heap-allocated Strings, it accesses their FNV-1a cached hash. For primitives (like numbers and booleans), it employs a fast, bit-shifting integer hash function.

### `void printValue(Value value)`
- **Description**: Prints the human-readable representation of the `Value` to standard output. If the value is a heap object, it delegates the formatting to `printObject`.
