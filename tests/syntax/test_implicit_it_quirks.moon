# Initial assignment
let x be 5

# The 'it' variable should automatically capture the left side of the modifier
set x to 10 if it < 10
  show x
# expect: 10

# 'it' should now represent the updated value of 10
add 20 to x unless it == 10
  show x
# expect: 10

# Testing 'it' with a phrasal update
update x * 2 if it == 10
  show x
# expect: 20
