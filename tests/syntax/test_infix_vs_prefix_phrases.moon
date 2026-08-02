##
  test_infix_vs_prefix_phrases.moon
  Tests for infix phrasal dispatch, prefix coexistence, and leading-tuple unpacking
##

# 1. Define Infix Phrasal Function (1 leading arg)
let (s: String) repeated (n: Number) times:
  let result be ""
  let count be 0
  while count < n:
    set result to result + s
    set count to count + 1
  end
  give result
end

# 2. Define Prefix Phrasal Function
let shout (msg: String):
  give "SHOUT: " + msg
end

# 3. Test Prefix Function Call
show shout("hello")
# expect: SHOUT: hello

# 4. Test Infix Function Call
show "Moon! " repeated 3 times
# expect: Moon! Moon! Moon! 

# 5. Test Coexistence: Prefix expression before an infix call
let message be "Hi! "
show message repeated 2 times
# expect: Hi! Hi! 

# 6. Test Infix Function with Leading Tuple (2 arguments)
let (x: Number, y: Number) point distance to (ox: Number, oy: Number):
  let dx be x - ox
  let dy be y - oy
  give dx * dx + dy * dy
end

let dist be (3, 4) point distance to (0, 0)
show "Distance squared: `dist`"
# expect: Distance squared: 25

# 7. Test Operator Overloading with Multi-Argument Tuples and Anchor
let (ax: Number, ay: Number) + (bx: Number, b_y: Number) offset:
  give (ax + bx) * 10 + (ay + b_y)
end

let combined be (1, 2) + (3, 4) offset
show "Vector sum encoding: `combined`"
# expect: Vector sum encoding: 46
