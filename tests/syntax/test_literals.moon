##
  test_literals.moon
  Tests numeric literals including hex, binary, and zero-padded numbers
##

show 0000x3
# expect: 3

show 00000b11010101
# expect: 213

show 00023134
# expect: 23134
