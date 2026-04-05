
## ==========================================
## MOON MASTER TEST SUITE
## ==========================================

show "--- 1. VARIABLES, MATH & CASTING ---"
let number be 10
let is_valid be true
let text be "42"

add 5 to number
update number * 2
set number to number mod 7

let parsed_num be text as Number
let stringified be is_valid as String

show "Math & Casting OK: `parsed_num`"


show "--- 2. CONTROL FLOW & MODIFIERS ---"
let status be "pending"

if number > 10:
    set status to "high"
else if number == 2:
    set status to "perfect"
else:
    set status to "low"
end

unless is_valid is false:
  set status to "validated"
end

# Statement Modifiers
set status to "overridden" if parsed_num is 42
give "Error!" unless status = "overridden"

show "Control Flow OK: `status`"


show "--- 3. LOOPS & RANGES ---"
let loop_sum be 0

# While loop with skip and break
let counter be 0
while counter < 10:
    update counter + 1
    skip if counter == 5
    break if counter == 8
    update loop_sum + counter
end

# For-each with a range and a custom step
for each i in 10 to 20 by 2:
    add i to loop_sum
end

show "Loops OK: `loop_sum`"


show "--- 4. COLLECTIONS & SLICING ---"
let heroes be ["Arthur", "Merlin", "Lancelot", "Gawain"]
let stats be {"level": 99, "class": "Mage"}

# 1-Based Indexing & Slicing
let first_hero be heroes[1]
let sub_team be heroes[2 to end]
let mage_level be stats["level"]

# Negative Indexing (Last item)
let last_hero be heroes[-1]

# In-place collection mutation
update heroes + ["Percival"]
update stats["level"] + 1

show "Collections OK: `sub_team` | Level: `stats["level"]`"


show "--- 5. MULTIPLE DISPATCH & UNION TYPES ---"

# Signature 1: Takes a List or Range
let process collection (iter: List or Range):
    let total be 0
    for each item in iter add item as Number to total
    give total
end

# Signature 2: Takes a String (Overloaded!)
let process collection (text: String):
    give uppercase text
end

let list_result be process collection [1, 2, 3]
let str_result be process collection "moon"

show "Multiple Dispatch OK: `list_result` | `str_result`"


show "--- 6. BLUEPRINTS & OOP ---"

type Player:
  name: "Unknown",
  health: 100,
  is_alive: true,
end

# Instantiation
let p1 be Player {
  name: "Munachi",
  health: 50
}

# The 'with' Keyword (Clone and Override)
let p2 be Player with
  name: "Emrys",
  health: 200
end

# Possessive Property Access & Mutation
update p1's health + 25
set p1's is_alive to false if p1's health < 0

show "Blueprints OK: `p1's name` (`p1's health` HP) vs `p2's name` (`p2's health` HP)"


show "--- 7. THE STANDARD LIBRARY ---"

# Math
let r be random from 1 to 10
let root be square root of 16
let pow be power of 2 to 3

# String
let clean_str be trim "   hello   "
let pieces be split "A,B,C" by ","

# List
let reversed be reverse [1, 2, 3]
let joined be join reversed with "-"
let popped be pop from pieces
let idx be index of "B" in pieces

show "StdLib OK: Root=`root`, Pow=`pow`, Joined=`joined`, Popped=`popped`"

show "=========================================="
show "ALL SYSTEMS NOMINAL. MOON IS READY."
show "=========================================="
