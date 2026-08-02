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
# expect: <multi-fn process$1 ([<union [<type List>, <type String>]>])>

show test2
# expect: <multi-fn test2$1 ([<type Any>])>
