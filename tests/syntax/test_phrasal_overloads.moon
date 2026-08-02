let do (x: Number) times (y: Number):
  give x * y
end

let do (x: Number) plus (y: Number):
  give x + y
end

# The VM should successfully traverse the Trie to find the right bytecode
show do 5 times 4
# expect: 20

show do 5 plus 4
# expect: 9
