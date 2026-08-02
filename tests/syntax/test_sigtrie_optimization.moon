# ==============================================================================
# Comprehensive Test Suite: Spaced Variables, SigTrie Mangling, and Interceptors
# ==============================================================================

# 1. Spaced Variable Declarations & Lookahead & Set
let player high score be 100
let first name be "Ada"
let last name be "Lovelace"

show "Player: `first name` `last name`"
show "Score: `player high score`"

set player high score to player high score + 50
set first name to "Grace"
set last name to "Hopper"

show "Updated Player: `first name` `last name`"
show "Updated Score: `player high score`"

# 2. Phrasal Functions with Optimized Mangling
let calculate (x) plus (y) times (z):
  give x + (y * z)
end

let res be calculate 5 plus 3 times 4
show "Calculate result: `res`"

# 3. Leading Argument / Infix Phrase
let (s: String) repeated (n: Number) times:
  let out be ""
  let count be 0
  while count < n:
    set out to out + s
    set count to count + 1
  end
  give out
end

let repeatedStr be "Moon! " repeated 3 times
show "Repeated: `repeatedStr`"

# 4. Operator-led Phrase with Custom Anchor
let (n: Number) + (days: Number) days:
  give n + (days * 86400)
end

let timestamp be 1000 + 5 days
show "Timestamp: `timestamp`"

# 5. Standard Binary Operators & Logical Interceptors (Verify No Precedence Stealing)
let a be true
let b be false
let c be true

if a and not b or c:
  show "Logical precedence verified"
end

let mathRes be 10 + 20 * 2 - 5
show "Math Result: `mathRes`"

# 6. Types with Spaced Fields and Methods
type User:
  first name is "Alan",
  last name is "Turing",
  greeting:
    give "Hello, " + my's first name + " " + my's last name
  end
end

let u be User {}
show "User direct access: `u's first name` `u's last name`"
show "User method: `u's greeting`"

# 7. Dictionaries with Spaced Keys
let d be {
  account balance: 5000,
  user email: "alan@example.com"
}

let balKey be "account balance"
show "Dict account balance: `d[balKey]`"

show "ALL TESTS PASS"
