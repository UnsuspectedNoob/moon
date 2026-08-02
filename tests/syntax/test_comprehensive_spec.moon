# 1. Spaced Variable Declarations & Lookahead & Set
let first name be "Ada"
let last name be "Lovelace"
let user score be 42

show "Full Name: `first name` `last name`"
# expect: Full Name: Ada Lovelace
show "Score: `user score`"
# expect: Score: 42

set first name to "Grace"
set last name to "Hopper"
set user score to user score + 58
show "Updated Name: `first name` `last name`"
# expect: Updated Name: Grace Hopper
show "Updated Score: `user score`"
# expect: Updated Score: 100

# 2. Phrasal Functions with Optimized Mangling
let calculate (x) plus (y) times (z):
  give x + (y * z)
end

let res be calculate 5 plus 3 times 4
show "Calculate 5 plus 3 times 4: `res`"
# expect: Calculate 5 plus 3 times 4: 17

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
# expect: Repeated: Moon! Moon! Moon! 

# 4. Operator-led Phrase with Custom Anchor
let (n: Number) + (days: Number) days:
  give n + (days * 86400)
end

let timestamp be 1000 + 5 days
show "Timestamp: `timestamp`"
# expect: Timestamp: 433000

# 5. Standard Binary Operators & Logical Interceptors (Verify No Precedence Stealing)
let a be true
let b be false
let c be true

if a and not b or c:
  show "Logical precedence works correctly!"
end
# expect: Logical precedence works correctly!

let mathRes be 10 + 20 * 2 - 5
show "Math Result: `mathRes`"
# expect: Math Result: 45

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
# expect: User direct access: Alan Turing
show "User method: `u's greeting`"
# expect: User method: Hello, Alan Turing

# 7. Dictionaries with Spaced Keys
let d be {
  account balance: 5000,
  user email: "alan@example.com"
}

let balKey be "account balance"
show "Dict account balance: `d[balKey]`"
# expect: Dict account balance: 5000

show "ALL TESTS COMPLETED SUCCESSFULLY!"
# expect: ALL TESTS COMPLETED SUCCESSFULLY!
