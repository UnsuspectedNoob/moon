##
  test_unions.moon
  Tests union types in phrasal signatures
##

let process (x: List or String or List):
  give x
end

let test2 (x: Number or Any):
  give x
end

show process
# expect: <process$1 (List or String)>

show test2
# expect: <test2$1 (Any)>
