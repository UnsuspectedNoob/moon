# GC Test: Cyclic References
# This script creates objects that reference each other in a cycle.
# Reference Counting garbage collectors leak memory here, but Mark-and-Sweep
# should correctly identify the entire island as unreachable and incinerate it.

let testCyclicGC:
  let a be { name: "Alice" }
  let b be { name: "Bob" }
  
  # The Cycle
  set a's partner to b
  set b's partner to a

  # The variables go out of scope and the island floats away...
end

# Generate thousands of abandoned cycles
for i in 1 to 50000:
  testCyclicGC
end

show "Finished cyclic test."
