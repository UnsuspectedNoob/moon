let list be [ 10, 20, 30 ]

show list[1]
# expect: 10

# Wrap around to the back
show list[ - 1]
# expect: 30

# Wrap all the way to the front using negatives
show list[ - 3]
# expect: 10

# The zero-index trap!
let crash be list[0]
# expect error: Remember, MOON lists start at index 1!
