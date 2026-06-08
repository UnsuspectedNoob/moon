show "--- INITIATING COMPREHENSION GAUNTLET ---"

let x be 999
let nested_lists be [ for each x in 1 to 3 keep [ for each x in 1 to x keep x * 10 ] ]

show "1. Shadowing & Nesting:
Expected Output: [[10], [10, 20], [10, 20, 30]]
Output: `nested_lists`"

let cube be [
  [ [ 1, 2 ], [ 3, 4 ] ],
  [ [ 5, 6 ], [ 7, 8 ] ]
]

let flat_evens be [
  for each slice in cube
    for each row in slice
      for each num in row
        keep num if num mod 2 is 0
]

show " 2. Matrix Flattener:
Expected Output: [2, 4, 6, 8]
Output: `flat_evens`"

let word be "abcd"
let char_map be {
  for each char, index in word
    keep uppercase char: index * 100
}

show "
3. Dictionary String Map:
Expected Output: {A: 100, B: 200, C: 300, D: 400}
Output: `char_map`"

let inventory be [ "sword", "shield", "potion", "bow" ]

let armory be {
  for each item, index in inventory:
    let is_weapon be item is not "potion"
    let loud_name be uppercase item

    keep loud_name: index unless not is_weapon
  end
}

show "
4. The Armory Filter:
Expected Output: {SWORD: 1, SHIELD: 2, BOW: 4}
Output: `armory`"

