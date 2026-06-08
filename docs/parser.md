
1. The Pre-Pass Architecture (The Lexical Scanner)
Before the AST is even built, the parser runs a scout mission to understand the shape of the codebase.
 The Phrasal Hoister (`hoistPhrases`): A recursive pre-pass function that scans the entire file to extract and map all phrasal function signatures (e.g., `let add (x) to (y)`) before standard parsing begins.
 Dynamic Module Resolution (`TOKEN_LOAD`): Resolves and loads external `.moon` files inline during the prepass to extract their verbs and add them to the global vocabulary.
 The Circular Dependency Shield (`isPrepassLoaded`): An array tracker that permanently prevents infinite recursive loops if two modules try to load each other.
 The Signature Mangler: Translates multi-word English function signatures into safe, O(1) Deterministic Finite Automaton (Trie) structures (e.g., mangling `add 5 to 10` into `add$1_to$1`).

2. State & Context Tracking
The parser maintains deep contextual awareness of exactly *where* it is in the code.
 The AST Depth Limiter (`parseDepth`): Tracks the nesting depth of expressions and automatically aborts to prevent C call-stack overflows (OS crashes) on excessively chained logic.
 Loop Depth Tracking (`loopingDepth`): A strict state counter that guarantees `break` and `skip` keywords throw syntax errors if a user tries to type them outside of a loop.
 The Blindfold State (`expectedLabelStack`): Tracks the `groupingDepth` of parentheses and brackets to stop the parser from aggressively eating English keywords that actually belong to outer phrasal functions.
 The Sticky Subject Capture (`currentStickySubject`): A memory register that captures the left-hand operand of a comparison, allowing for seamless chained conditionals (e.g., `if health is 100 or is 50`).

3. The Pratt Expression Engine
A robust Top-Down Operator Precedence engine that evaluates math and logic based on strict binding power.
 Prefix & Infix Routing: Dynamically routes tokens to their designated parsing functions based on the `ParseRule` table.
 Left-Associativity Enforcement: Uses a `+ 1` precedence trick (in `binary` and `castExpression`) to ensure operations like casting (`a as b as c`) chain from left to right mathematically.
 Ternary Evaluation: Parses inline conditionals (`expr if cond else expr`).

4. Data Types & Literals
 Primitives: `number`, `true`, `false`, and `nil`.
 Strings: Standard double-quoted literals.
 String Interpolation (`interpolation`): Parses nested expressions inside backtick `` ` `` blocks.
 Lists (`list`): Parses bracketed arrays, automatically routing to Comprehensions if the `for` keyword is detected.
 Dictionaries (`dict`): Parses key-value pairs. Automatically converts unquoted identifiers (e.g., `name: "Munachi"`) into string keys behind the scenes.
 Ranges (`range`): Parses `start to end by step` syntax, with an optional `by` clause that defaults to `1.0`.

5. Variables & Memory Access (L-Values)
 Variables (`variable`): Parses standard identifiers. Also acts as the execution trigger for Phrasal Calls.
 Intrinsic Shields: Blocks users from naming variables with the `__` prefix, strictly reserving them for VM C-primitives.
 Property Access (`dot`, `possessive`): Handles both standard `obj.prop` and the MOON-specific `obj's prop` syntax.
 Subscript Access (`subscript`): Handles index parsing `list[1]`.
 The `end` Keyword: A context-aware literal that represents the final index of the active collection being subscripted.

6. Control Flow & Comprehensions
 Standard Branching (`ifStatement`): Parses `if` and `unless` (inverted if) blocks, along with their `else` fallbacks.
 Loops (`whileLogic`, `forStatement`): Parses standard `while`, `until` (inverted while), and iterator-based `for each` loops.
 List Comprehensions (`listComprehension`): Parses both Expression-Mode (`keep x`) and Block-Mode (`:`) data generation, with optional index tracking (`for each item, i in list`).
 Dictionary Comprehensions (`dictComprehension`): Parses dynamic hash map generation (`keep key : value`).

7. Actions & Mutations (Statements)
 Variable Declarations (`letDeclaration`): Parses `let x be 10` and multi-assignments `let x, y be 1, 2`. Explicitly rejects `=` in favor of `be`.
 Assignments (`setStatement`): Parses `set x to 10`. Handles complex L-Value targets like `set player's inventory[0] to "Sword"`. Explicitly rejects `=`.
 In-Place Additions (`addStatement`): Parses `add 5 to score` and desugars it directly into a standard assignment AST.
 In-Place Updates (`updateStatement`): Parses `update x * 5` or `update x as String`. Uses hidden ghost variables (`" obj"`, `" idx"`) to safely mutate complex properties without re-evaluating the parent objects.
 Returns (`giveStatement`): Parses the `give` keyword to return values from functions.
 Module Loading (`loadStatement`): Parses `load "file.moon"`.

8. AST Inversion (Statement Modifiers)
 Postfix Logic: Automatically intercepts trailing conditionals (e.g., `give 5 if true` or `set x to 10 unless false`) attached to standard expressions, assignments, or actions.
 The Bubble-Up Fix: Physically extracts the modifier from the deepest evaluated node (like a function argument) and wraps it around the entire parent statement.

9. The Type System & Object Orientation
 Type Blueprints (`typeDeclaration`): Parses `type Name:` blocks and captures all default property values.
 Type Annotations (`parseTypeAnnotation`): Extracts type requirements for function parameters.
 Union Types: Parses `Type A or Type B` and compiles them into a unified `NODE_UNION_TYPE`.
 Instantiation (`instantiate`): Parses `{prop: value}` blocks to create clones of Blueprints.
 Overrides (`instantiateWith`): Parses `variable with prop: value end` to duplicate and modify existing instances.
 Type Casting (`castExpression`): Parses the `as` keyword to transition between Types at runtime.

10. The Empathy & Diagnostic Engine
 Panic Mode Recovery (`synchronize`): When a syntax error occurs, the parser stops throwing cascading errors and silently fast-forwards to the next major keyword to resume clean parsing.
 Targeted Hints (`consumeHint`): Replaces generic "Expected Token" errors with plain-English explanations of *why* the token is required.
 The Block Closer (`consumeBlockEnd`): Remembers exactly what type of block was opened (e.g., a function vs. a loop) to provide highly specific error messages if the `end` keyword is missing.

11. The Language Server Protocol (LSP) Shield
 The State Purge (`resetParserState`): Wipes all active global variables, blindfolds, and trie structures clean before every keystroke, allowing the LSP to parse a live, changing document without stale memory corruption.
 The Standard Library Rescue: Automatically re-injects the MOON core libraries (`<math>`, `<string>`, etc.) into the parser's brain after a purge.
 The AST Rescue: Bypasses the compiler's strict failure states. Even if a script is broken and `hadError` is triggered, the parser still forces the AST to return a `NODE_BLOCK` so the LSP can traverse the broken tree and provide autocomplete for surviving variables.
