
# Fundamental Features Audit (Moon 1.0)

This audit is a systematic review of the Moon language architecture to identify critical missing features required for a robust 1.0 production release. 

Moon's English-like expressiveness and powerful phrasal functions provide a stunning foundation. However, to be production-ready for general-purpose programming, the language is currently missing several standard architectural pillars.

## Open Questions
> [!IMPORTANT]
> Please review the missing features below. Which of these should we prioritize implementing first for the 1.0 release? 

## 1. Error Recovery (Try/Catch)
Currently, `throwNativeError` instantly halts the VM with `exit(70)`. There is zero user-space error recovery.
- **Impact**: Any I/O error (like reading a missing file) or logic error (out-of-bounds index) takes down the entire application.
- **Proposed Solution**: Introduce a native error handling mechanism. Given Moon's English-like syntax, we could introduce `attempt / recover` blocks.
```moon
attempt:
  let data be load_file("config.json")
recover error:
  show "Failed to load config: `error`"
end
```

## 2. First-Class Functions & Closures
Phrasal functions are currently globally registered in a Trie (`sigtrie.c`). There is no mechanism to pass a function as a variable, return a function, or define an anonymous inline callback.
- **Impact**: Blocks the implementation of advanced functional paradigms, asynchronous callbacks, and higher-order functions.
- **Proposed Solution**: Introduce anonymous block closures using a `do` or `|args|` syntax that can capture the lexical scope.

## 3. Encapsulation & Exports (Public/Private)
When a file is loaded via `load "utils.moon" as utils`, all variables, types, and phrasal functions defined inside it are implicitly accessible.
- **Impact**: Makes it impossible to hide internal module state or build robust, encapsulated library APIs.
- **Proposed Solution**: Introduce `public` and `private` visibility modifiers for variables, types, and functions. Alternatively, require an explicit `export` statement at the bottom of a module.

## 4. Enums & Pattern Matching
While Moon supports Union Types (`String | Number`), it completely lacks strict, named Enums (Algebraic Data Types) and exhaustiveness checking.
- **Impact**: State-machine logic and strict categorization require "stringly-typed" workarounds.
- **Proposed Solution**: Introduce an `enum` keyword and a `match` control flow structure.

## 5. Type Methods / Object Orientation
Custom types are currently just "blueprints" (data containers). Phrasal functions operate on them globally, but you cannot attach behavior directly to a type.
- **Impact**: Weakens object-oriented paradigms and clutters the global phrasal namespace.
- **Proposed Solution**: Allow phrasal functions or standard functions to be scoped directly inside a `type` block, granting them implicit access to `self`.

## 6. Concurrency (Async/Await)
There is no native thread or coroutine management.
- **Impact**: I/O operations block the entire VM thread.
- **Proposed Solution**: Introduce native Coroutines / Fibers under the hood, exposed via `async` blocks and `await` keywords.

## 7. Bitwise Operators
The scanner and parser completely lack bitwise operators (`&`, `|`, `^`, `<<`, `>>`).
- **Impact**: Prevents users from writing low-level systems, cryptographic algorithms, or custom hashing logic.
- **Proposed Solution**: Add standard bitwise operators, perhaps using English keywords like `bit_and`, `bit_or`, `shift_left` to maintain the language's theme.
