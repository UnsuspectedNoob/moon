show "--- INITIATING COMPREHENSION GAUNTLET ---"

# TEST 1: The Shadow Realm (Scope & Nesting)
# We use the variable 'x' globally, then as an outer iterator, then as an inner iterator!
# If your Lexical Scoping or Ghost Variables leak, this will instantly panic or produce garbage.
let x be 999
let nested_lists be [for each x in 1 to 3 keep [for each x in 1 to x keep x * 10]]

show "1. Shadowing & Nesting:
Expected Output: [[10], [10, 20], [10, 20, 30]]
Output: `nested_lists`"
# Expected Output: [[10], [10, 20], [10, 20, 30]]


# TEST 2: The Matrix Flattener (Block Mode & Math Precedence)
# We take a 3D matrix, drill down using standard loops inside a comprehension,
# and use the AST Inverted 'keep ... if' to extract only the evens.
let cube be [
  [ [1, 2], [3, 4] ],
  [ [5, 6], [7, 8] ]
]

let flat_evens be [for each slice in cube:
  for each row in slice:
    for each num in row:
      keep num if num mod 2 is 0
    end
  end
end]

show "
2. Matrix Flattener:
Expected Output: [2, 4, 6, 8]
Output: `flat_evens`"
# Expected Output: [2, 4, 6, 8]


# TEST 3: The String Slicer (Dictionary Comprehension & Indices)
# Iterating over a string, extracting the characters and their indices,
# and dynamically building a dictionary key-value pair!
let word be "abcd"
let char_map be {for each char, index in word keep uppercase char : index * 100}

show "
3. Dictionary String Map:
Expected Output: {A: 0, B: 100, C: 200, D: 300}
Output: `char_map`"
# Expected Output: {A: 0, B: 100, C: 200, D: 300}


# TEST 4: The Final Boss (Mixed Modes & Logical Inversion)
# A block-mode dictionary comprehension that uses local variables inside the loop,
# calls native string functions, and uses the 'unless' modifier on a 'keep' statement.
let inventory be ["sword", "shield", "potion", "bow"]

let armory be {for each item, index in inventory:
    let is_weapon be item is not "potion"
    let loud_name be uppercase item
   
    keep loud_name : index unless not is_weapon
end}

show "
4. The Armory Filter:
Expected Output: {SWORD: 0, SHIELD: 1, BOW: 3}
Output: `armory`"
# Expected Output: {SWORD: 0, SHIELD: 1, BOW: 3}
