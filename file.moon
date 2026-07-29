##
  test_concat.moon
  Benchmarking Moon's O(1) Rope Architecture
##

show "Starting MOON string concatenation..."
let start be clock

let text be "moon"
let i be 0

while i < 5000:
  set text to "a" + text + "b"
  update i + 1
end

let stop be clock

# We only measure the time it took to build the tree!
show "Final String Length: `text's length` characters"
show "Time Taken: `stop - start` seconds"
