let a be 1
let b be 5
let c be 10

# Standard mathematical chain
if a < b <= 5 < c:
  show "Math works!"
end
# expect: Math works!

# Mixing equality and less-than
show a == 1 < b
# expect: true

# Short-circuit test (b < a is false, so it shouldn't evaluate c)
show c > b < a < c
# expect: false
