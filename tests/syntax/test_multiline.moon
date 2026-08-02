##
  test_multiline.moon
  Tests multiline string interpolation
##

let name be "Munachi"
let s be '''Hello `name`!
Welcome to Moon.'''
show s
# expect: Hello Munachi!
# expect: Welcome to Moon.
