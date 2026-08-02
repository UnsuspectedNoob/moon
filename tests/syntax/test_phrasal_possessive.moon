##
  test_phrasal_possessive.moon
  Tests extension phrasal methods with possessive receiver and multiple arguments
##

let (n: Number)'s falls between (a: Number) and (b: Number):
  show "a is `a`, b is `b`"
  give n >= a and n <= b
end

show 5's falls between 1 + 1 and 10
# expect: a is 2, b is 10
# expect: true
