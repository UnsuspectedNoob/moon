# Test variable and multi-function coexistence under same root name

let stuff (n):
  show "stuff"
end

let stuff (n: Number):
  show "number stuff"
end

let stuff be 3

# Reference the variable
show stuff
# expect: 3

# Call multi-function overloads
stuff 3
# expect: number stuff

stuff "hello"
# expect: stuff

# Use variable in math expression
show stuff + 10
# expect: 13

# Reverse definition order: variable first, then multi-function
let item be 100

let item (x: Number) more:
  give item + x
end

show item
# expect: 100

show item 25 more
# expect: 125
