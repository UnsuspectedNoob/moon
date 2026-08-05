##
  test_multi_spaced_let.moon
  Tests multiple variable declarations supporting spaced identifiers.
##

let first name, last name be "Munachiso", "Ukpai"
show first name
# expect: Munachiso
show last name
# expect: Ukpai

let a, middle name, c be 1, "Grace", 3
show a
# expect: 1
show middle name
# expect: Grace
show c
# expect: 3

let player score, high score be 100
show player score
# expect: 100
show high score
# expect: 100

set first name, last name to "Munachi", "U."
show first name
# expect: Munachi
show last name
# expect: U.

let test scope:
  let local first, local last be "Hello", "World"
  give "`local first` `local last`"
end
show test scope
# expect: Hello World
