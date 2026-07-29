# The Moon Language: Definitive Syntax Reference

This document serves as the absolute source of truth for the Moon programming language syntax. Every rule, keyword, and code example in this document is derived directly from the C compiler (`scanner.c` and `parser.c`). 

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
- **Strings:** Must be wrapped in double quotes `"..."`.
 there is no double up, you must confirm where you saw this.
- **Escaping Quotes:** Use standard `\"` or double up `""` (e.g., `"He said ""Hello"""`).
- **Interpolation:** Wrap expressions inside strings using backticks `` `...` ``. The compiler jumps back into expression mode inside the backticks.

```moon
let base be 10
# The `base * 2` is parsed dynamically!
let message be "The doubled value is `base * 2`."
```

### 1.4 Comments
Comments begin with a hash `#` and extend to the end of the line.

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
To create an instance, use curly braces `{}` and specify the properties via colons.
```moon
let hero be Player { name: "Arthur", health: 150 }
```

@@ the idea here is wrong, the 'with' does the same thing as the {}, although I think your idea here is better than what is currently implemented. Let's do that. 
To dynamically override properties on an existing object, use the `with` operator:
```moon
let poisonedHero be hero with:
  health: 10
end
```

### 2.4 Type Casting
You can enforce or cast types dynamically using `as`.
```moon
let str_val be 100 as String
```

---

## 3. Phrasal Methods (The SigTrie Layer)

Moon functions are parsed using a Signature Trie. This allows functions to have multi-word names with arguments injected directly into the phrase.

### 3.1 Function Declarations
Functions are declared using `let`. Arguments are defined in parentheses `(name: Type)`. The phrase must end with a colon `:`.

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

### 3.2 Extension Methods
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
- **Lists:** Created with square brackets. Items separated by commas. `[1, 2, 3]`
- **Dictionaries:** Created with curly braces. Keys can be any valid Moon value. `{ name: "Munachi", "age": 21 }`
- **Subscript & Dot Access:** Retrieve properties via `list[0]` or `player.health`.
- **Possessives:** You can use English possessive grammar instead of dots: `player's health`.

### 4.2 Operator Precedence
From highest priority to lowest:
1. `()` (Grouping), `[]` (List/Subscript), `{}` (Dict/Instantiate), `.` (Dot), `'s` (Possessive)
2. `not`, `!` (Logical NOT), `-` (Math Negation)
3. `as` (Type Cast)
4. `*`, `/`, `mod` (Factor Math)
5. `+`, `-` (Term Math)
6. `>`, `>=`, `<`, `<=` (Comparison)
7. `is`, `==`, `=` (Equality)
8. `and` (Logical AND)
9. `or` (Logical OR)
10. `to` (Range Generation, e.g., `1 to 10`)

### 4.3 Chained & Sticky Comparisons
- **Chained:** `1 < x <= 10` is supported dynamically.
@@ we need to go over this again in the code.
- **Sticky:** Prefixing an expression with a comparison (e.g. `is 10` or `> 50`) will implicitly bind the left-hand side to `it` or the current context iterating variable.

---

## 5. Action Statements

Moon relies on specific verbs to manipulate memory instead of abstract symbols like `=`. 

### 5.1 Assignment & Mutation
```moon
# Assignment
set score to 100

# In-Place Update (Math required: + - * /)
update score + 50
```

### 5.2 List & Return Actions
```moon
# Pushing to lists
@@ add isn't just pushing to lists..
add item to inventory

# Yielding / Returning
give "Operation Complete"

# Early Exit / Loop Control
break
skip
quit
```

### 5.3 Comprehension Yielding
When building comprehensions, `keep` acts as a conditional yielder.
```moon
# Creates a new list containing only even numbers
@@ you omitted the [] that surround the comprehension
let evens be for each x in numbers:
  keep x if x mod 2 == 0
end
```

---

## 6. Control Flow & Blocks

All control flow blocks must open with a colon `:` and terminate with the `end` keyword.

### 6.1 Conditionals
```moon
if health > 80:
  show "Healthy"
else if health > 20:
  show "Hurt"
else:
  show "Critical"
end

# Inverted logic
unless enemy_dead:
  attack enemy
end
```

### 6.2 Loops
```moon
# Standard While
while game_active:
  update timer - 1
end

# Inverted While (Until)
until boss_dead:
  attack boss
end
```

### 6.3 Iterators & Comprehensions
The `for` loop has a very strict grammatical structure: 
`for (each)? <iterator> (, <index>)? (in|from) <sequence>:`

```moon
# Standard iteration
for each item in inventory:
  show item
end

# Iteration with index
for each item, i in inventory:
  show "Item \`i\`: \`item\`"
end
```
@@ this is somehow wrong like I mentioned above
*Note: Using `for` without a colon block directly assigns the loop result to an expression (Comprehensions).*

### 6.4 Statement Modifiers
Any single-line statement can become conditional by suffixing it with `if` or `unless`.
```moon
break if health == 0
give "winner" unless score < 100
set status to "over" if game_ended
```

@@ I would also like you to mention the ternary operator, in the right place in the file through.
