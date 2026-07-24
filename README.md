# Moon Engine 🌙

Moon is a dynamically-typed, highly expressive programming language built from scratch in C. Its syntax revolves around readability, mirroring natural human thought and English phrasing, while compiling down to a blazing-fast, custom bytecode virtual machine.

This repository contains the core C engine, including the lexical scanner, Pratt parser, Signature Trie, AST generation, Bytecode Emitter, and the Virtual Machine (VM).

## Features
- **Natural Phrasal Syntax**: Moon uses English phrasing (`let x be 10`, `set score to 100`, `update score + 50`).
- **Function Signatures**: Forget standard `func(a, b)`. Moon parses function phrases with arguments inline (e.g. `let attack (enemy: Enemy) with (weapon: Weapon):`).
- **Dynamic Blueprints (Types)**: First-class object creation using `type` declarations, `with` clauses for overrides, and structural duck-typing.
- **Blazing Fast**: Hand-written in C99 with no external dependencies (aside from standard libraries and cJSON for the LSP).
- **WebAssembly Ready**: The entire language, AST parser, and VM can be cross-compiled seamlessly into WASM via Emscripten to run in the browser.
- **Built-in Language Server**: Contains a native LSP (`moon_lsp`) integrated right into the compiler core for editor support.

## Getting Started

### Prerequisites
- `gcc` or `clang`
- `make`

### Building from Source

To compile the native executable, simply run:
```bash
make
```

To compile with aggressive optimizations for release:
```bash
make release
```

This will produce the `moon` executable in the root directory.

### Running Moon Code

Run a `.moon` file by passing it to the engine:
```bash
./moon file.moon
```

Or start the REPL (Interactive Prompt) if no file is provided:
```bash
./moon
```

### Compiling to WebAssembly

If you have the Emscripten SDK (`emsdk`) installed, you can compile the engine to WASM for use in the browser (like the [moon-web](https://github.com/UnsuspectedNoob/moon-web) playground).

```bash
./build_wasm.sh
```
This generates `moon.js` and `moon.wasm` equipped with `ASYNCIFY` for yielding execution to the browser event loop.

## Architecture & Codebase Navigation

The engine follows a standard single-pass compilation pipeline to AST, then a secondary pass to Bytecode:

- `src/scanner.c`: Lexical analysis (Tokenizer)
- `src/parser.c`: Pratt-style AST generator and Syntax error handling
- `src/sigtrie.c`: The Signature Trie handling multi-word phrasal function resolution
- `src/compiler.c` / `src/codegen.c`: AST validation and Bytecode emitting
- `src/vm.c`: The custom Bytecode Virtual Machine executing operations
- `src/object.c` / `src/value.c`: NaN-tagging value representation and garbage-collected objects
- `src/lsp.c`: The built-in Language Server Protocol implementation

## Testing
There are numerous test `.moon` files in the root and `tests/` directories.
To run performance tests and benchmarks:
```bash
./moon bench_call.moon
```

## Syntax Quick Start
Take a look at `SYNTAX.md` for a complete reference, but here is a quick taste:

```moon
# Variables
let hero be "Arthur"
let health be 100

# Conditionals
if health > 50:
  show "You are strong."
else:
  show "You need a potion."
end

# Types and Objects
type Player:
  name: "Unknown",
  hp: 100
end

let p be Player { name: hero }

# Phrasal Functions
let heal (target: Player) by (amount: Number):
  update target.hp + amount
end

heal p by 50
```

## License
MIT License
