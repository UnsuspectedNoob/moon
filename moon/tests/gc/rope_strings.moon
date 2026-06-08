# GC Test: Rope Strings
# This script massively tests String concatenations to ensure that when a
# gigantic "Rope" tree structure goes out of scope, the GC traverses its
# left and right branches properly and reclaims all memory without crashing.

let testRopeGC:
  let str be "hello"
  for i in 1 to 5000:
    update str + " world"
  end
  # The 5,000-node Rope tree is now abandoned.
end

for i in 1 to 200:
  testRopeGC
end

show "Finished rope test."
