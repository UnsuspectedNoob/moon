## =======================================================
## M.O.O.N. Language - Comprehensive Syntax Overview
## =======================================================

# 1. VARIABLES & MUTATION
let greeting be |Hello, World!|
let score be 0
let isActive be true
let nothingness be nil

set score to 100                 # Standard reassignment
update score + 50                # In-place math (+, -, *, /, mod)
update greeting as String        # In-place type casting

# 2. STRINGS & INTERPOLATION
# Strings are enclosed in vertical pipes. Double pipes || act as an escaped pipe.
let name be |Aegus|
let welcome be |Greetings `name`, welcome to MOON!|||

# 3. DATA STRUCTURES
let fibonacci be [1, 1, 2, 3, 5, 8]
let range_list be 1 to 10 by 2

let user_profile be {
  |username|: |Munachi|,
  |level|: 42,
  |inventory|: [|Sword|, |Shield|]
}

# 4. INDEXING & PROPERTIES
let third_num be fibonacci[3]          # 1-based indexing!
let sub_list be fibonacci[2 to 4]      # Slicing with ranges
let user_name be user_profile[|username|]
let user_level be user_profile's level # Possessive access

# 5. CONTROL FLOW (Block & Inline)
if score > 100:
  show |High Score!|
else if score == 100:
  show |Perfect!|
else:
  show |Keep trying.|
end

unless isActive:
  show |System is offline.|
end

# Ternary & Statement Modifiers
let status be |Online| if isActive else |Offline|
give score if score > 0


# 6. LOOPS & ITERATION
let counter be 10
while counter > 0:
  update counter - 1
  skip if counter mod 2 == 0 # Continue to next iteration
  break unless isActive
end

until counter is 10:
  update counter + 1
  quit if counter > 100      # Break out of loop (alias for break)
end

# For Loops (Lists, Ranges, Strings)
for each item, index in fibonacci:
  show |Item `index` is `item`|
end

# For Loops (Dictionaries)
for each key, value in user_profile:
  show |`key` -> `value`|
end

# 7. COMPREHENSIONS
let evens be [for each n in 1 to 10 keep n * 2]
let filtered be [for each n in fibonacci keep n if n > 2]

let flipped_dict be {for each k, v in user_profile keep v: k}
let block_comp be {for each x in [1, 2, 3]:
  let doubled be x * 2
  keep doubled
end}

# 8. BLUEPRINTS (Object-Oriented Programming)
type Vector3:
  x: 0.0,
  y: 0.0,
  z: 0.0
end

type Player:
  name: |Unknown|,
  health: 100,
  position: Vector3 {} # Nested default instantiation
end

# 9. INSTANTIATION & OVERRIDES
let p1 be Player {
  name: |Hero|,
  health: 250
}

# The 'with' keyword allows inline, single-line overrides
let p2 be Player with name: |Villain| end

# 10. FUNCTIONS & MULTIPLE DISPATCH
let jump:
  show |Jumping!|
end

let calculate sum of (a: Number) and (b: Number):
  give a + b
end

let process (data: String or List):
  show |Processing a string or a list!|
end

# 11. THE STANDARD LIBRARY (Built-ins)

# Core & I/O
show |Print to console|
let input be ask |What is your name? |
let time be clock
load |iter.moon|

# Math Library
let trig be sin 1.0 + cos 1.0
let root be square root of 16
let exponent be power of 2 to 8
let rounded be floor of 4.9
let rng be random from 1 to 100

# String Library
let big_str be uppercase |moon|
let smol_str be lowercase |MOON|
let clean be trim |  spaces  |
let words be split |a,b,c| by |,|

# List Library
let reversed be reverse [1, 2, 3]
let sentence be join words with | |
let popped_val be pop from reversed
let idx be index of 2 in [1, 2, 3]

# Type Conversion & Parsing
let base_ten be numbers in |1010| in base 2
let str_num be |42| as Number
let type_reflection be p1 as Type # Returns the Blueprint 'Player'
