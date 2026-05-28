let count be 0
while count < 3:
  add 1 to count
  show "While: " + count
end

let j be 3
until j == 0:
  set j to j - 1
  show "Until: " + j
end

for i from 1 to 3 by 1:
  show "For: " + i
end

let items be ["apple", "banana"]
for each item in items:
  show "Each: " + item
end

# expect: While: 1
# expect: While: 2
# expect: While: 3
# expect: Until: 2
# expect: Until: 1
# expect: Until: 0
# expect: For: 1
# expect: For: 2
# expect: For: 3
# expect: Each: apple
# expect: Each: banana
