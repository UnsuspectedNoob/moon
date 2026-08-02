##
  test_alias.moon
  Tests type aliases with union types
##

type p is List or String or List
show p
# expect: <union [<type List>, <type String>]>

let check (s: p):
  give s
end

show check([1, 2, 3])
# expect: [1, 2, 3]

show check("works!")
# expect: works!
