# The Moon Language: Definitive Syntax Reference

This document serves as the absolute source of truth for the Moon programming language syntax. Every rule, keyword, and code example in this document is derived directly from the C compiler (`scanner.c`, `parser.c`, and `sigtrie.c`).

---

## 1. Lexical Rules (The Scanner Layer)

Moon code is broken down into tokens before parsing. The Lexer enforces the following rules:

### 1.1 Reserved Keywords
Moon reserves exactly 35 keywords. These cannot be used as variable, type, or property names.
```text
add, and, as, be, by, break, each, else, end, false, for, from, 
give, if, in, is, it, keep, let, load, nil, not, or, quit, set, 
skip, then, to, true, type, unless, until, update, while, with
```

### 1.2 Data Literals
- **Numbers:** Moon natively supports decimal (`42`, `3.14`), hexadecimal (`0xFF`), and binary (`0b1010`) formats.
- **Booleans & Nil:** `true`, `false`, and `nil` representing nothingness.

### 1.3 Strings & Interpolation
Moon strings are highly robust, supporting multi-level inline interpolation.
- **Single-Line Strings:** Wrapped in double quotes `"..."`. Standard escape sequences like `\"`, `\n`, `\t` are supported.
- **Multiline Strings:** Enclosed in triple single-quotes `'''...'''`.
- **Interpolation:** Wrap expressions inside strings using backticks `` `...` ``. The compiler enters expression parsing mode inside the backticks.

```moon
let base be 10
# The `base * 2` is evaluated dynamically inside the string!
let message be "The doubled value is `base * 2`."
```

### 1.4 Comments
- **Single-Line Comments:** Begin with a hash `#` and extend to the end of the line.
- **Multiline Comments:** Enclosed in double hash blocks `## ... ##`.

```moon
# This is a single-line comment

##
  This is a multiline comment block
  spanning multiple lines of text.
##
```

---

## 2. Variables & Types (The Declaration Layer)

### 2.1 Variable Binding
Variables are declared using the `let` and `be` keywords.
```moon
let age be 30
let name be "Emrys"
```

### 2.2 Type Declarations (`type`)
Moon supports object blueprints via the `type` keyword. Types begin with a colon `:` and list properties (with default values or `nil`) separated by commas, closed by `end`.

```moon
type Player:
  name: "Unknown",
  health: 100,
  stamina: 50
end
```
**Type Aliases:** `type Alias is TargetType`

### 2.3 Instantiation & Overrides
To create an instance, use curly braces `{}` and specify the properties via colons:
```moon
let hero be Player { name: "Arthur", health: 150 }
```

To create a modified clone of an existing object, use the `with` keyword:
```moon
let poisonedHero be hero with:
  health: 10
end
```

### 2.4 Type Casting
You can enforce or cast types dynamically using `as`:
```moon
let str_val be 100 as String
```

---

## 3. Phrasal Methods (The SigTrie Layer)

Moon functions are parsed using a Signature Trie DFA. This allows functions to have multi-word names with arguments injected directly into the phrase.

### 3.1 Prefix Phrasal Functions
Functions starting with an identifier word are declared using `let`. Arguments are defined in parentheses `(name: Type)`. The phrase must end with a colon `:`.

```moon
# Zero-Arity (No Arguments)
let jump:
  show "Jumping!"
end

# Multi-Word Phrasal Signature
let attack (enemy: Enemy) with (weapon: Weapon):
  update enemy's health - weapon's damage
end
```

### 3.2 Infix Phrasal Functions & Operator Overloads
Phrases starting with an argument `(arg: Type)` act as infix phrases or overloaded operators.
- Infix phrases are dispatched with precedence `PREC_PHRASE` (9).
- Phrasal operators require a clarifying identifier anchor (e.g. `let (a) + (b) offset:`).

```moon
# 1-Leading Argument Infix Phrase
let (s: String) repeated (n: Number) times:
  let result be ""
  let count be 0
  while count < n:
    set result to result + s
    set count to count + 1
  end
  give result
end

let shout be "Moon! " repeated 3 times

# Multi-Argument Leading Tuple Infix Phrase
let (x: Number, y: Number) point distance to (ox: Number, oy: Number):
  let dx be x - ox
  let dy be y - oy
  give dx * dx + dy * dy
end

let dist be (3, 4) point distance to (0, 0)
```

### 3.3 Extension Methods
You can attach methods to a specific receiver type using possessives (`'s`) or grouping `(receiver)`.

```moon
# Option A: Standalone Extension Method
let (p: Player)'s heal (amount: Number):
  update p.health + amount
end

# Option B: Internal 'my' block inside a Type Declaration
type Enemy:
  hp: 100,
  my info:
    give "Health: `my hp`"
  end
end
```

---

## 4. Expressions & Data Structures (The Pratt Parser)

### 4.1 Data Collections
- **Lists:** Dynamic, mutable arrays created with square brackets `[...]`. Items separated by commas: `[1, 2, 3]`
- **Dictionaries:** Key-value hash maps created with curly braces `{...}`: `{ name: "Munachi", "age": 21 }`
- **Tuples:** Transient compile-time argument groupings created with parentheses `(...)`: `(x, y)`
- **Subscript & Dot Access:** Retrieve properties via `list[0]` or `player.health`.
- **Possessives:** Use natural English possessive grammar: `player's health`.

### 4.2 Operator Precedence
From highest priority to lowest:
1. `()` (Grouping / Tuples), `[]` (List/Subscript), `{}` (Dict/Instantiate), `.` (Dot), `'s` (Possessive)
2. `not`, `!` (Logical NOT), `-` (Math Negation)
3. `as` (Type Cast)
4. `*`, `/`, `mod` (Factor Math)
5. `+`, `-` (Term Math)
6. `>`, `>=`, `<`, `<=` (Comparison)
7. `is`, `==`, `=` (Equality)
8. `and` (Logical AND)
9. `or` (Logical OR)
10. Infix Phrasal Dispatch (`PREC_PHRASE`)
11. `to` (Range Generation, e.g., `1 to 10`)

### 4.3 Ternary Expressions vs Statement Modifiers
Moon distinguishes between ternary expressions producing values and statement modifiers controlling execution.

- **Ternary Expression:** `<expr> if <condition> else <expr>`
  ```moon
  let discount be 20 if is_vip else 0
  let greeting be "Hello, " + ("Sir" if is_formal else "Friend")
  ```
- **Statement Modifier:** `<statement> if <condition>` (or `unless`)
  ```moon
  show "Access granted" if is_admin
  quit unless connection_active
  ```

### 4.4 Chained & Sticky Comparisons
- **Chained Comparisons:** `1 < x <= 10` is supported.
- **Sticky Comparisons:** Prefixing an expression with a comparison operator (e.g. `is 10` or `> 50`) binds the left-hand side to `it` or the iterating collection element.

---

## 5. Action Statements

Moon relies on expressive verbs for memory operations instead of abstract symbols.

### 5.1 Assignment & Mutation
```moon
# Reassignment
set score to 100

# In-Place Numeric Update (+ - * /)
update score + 50
```

### 5.2 Collection & String Mutation (`add`)
The `add` verb performs append and accumulation operations:
```moon
# Appending to lists
add item to inventory

# Accumulating numbers
add 10 to total_score

# Concatenating strings
add " World" to greeting
```

### 5.3 Yielding & Control
```moon
give "Result"  # Returns a value from a function
break          # Exits the current loop
skip           # Skips to the next iteration
quit           # Terminates program execution
```

### 5.4 Comprehensions
List comprehensions are enclosed in brackets `[...]` and use `keep` to conditionally collect items:
```moon
let evens be [for each x in numbers:
  keep x if x mod 2 == 0
end]
```

---

## 6. Control Flow & Blocks

Moon supports both structured multi-line blocks (`: ... end`) and concise single-statement inline forms.

### 6.1 Conditionals
```moon
# Single-Statement Inline
if score > 50 show "Passed"

if score > 50 show "High"
else show "Low"

# Multi-Line Block
if health > 80:
  show "Healthy"
else if health > 20:
  show "Hurt"
else:
  show "Critical"
end

# Inverted Condition (unless)
unless enemy_dead:
  attack enemy
end
```

### 6.2 Loops
```moon
# While Loop
while game_active:
  update timer - 1
end

# Single-Statement While Loop
while count < 5 set count to count + 1

# Until Loop (Inverted While)
until boss_dead:
  attack boss
end
```

### 6.3 Iterators
The `for` loop syntax: `for (each)? <iterator> (, <index>)? (in|from) <sequence>:`
```moon
for each item in inventory:
  show item
end

for each item, i in inventory:
  show "Item `i`: `item`"
end
```
