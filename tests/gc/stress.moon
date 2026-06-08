# GC Test: Stress the Allocator
# This test bombards the VM's heap by constructing and abandoning massive 
# collections continuously. If the Mark-and-Sweep logic has a memory leak, 
# the host OS will violently kill this script (OOM) before it finishes.

let allocateTrash:
  let bigList be [ 1 to 50000 ]
  let dict be { key: "value", age: 100 }
  
  for i in 1 to 50000:
    add dict to bigList
  end
  # Memory peaks, and then the function exits.
end

for i in 1 to 5:
  allocateTrash
end

show "Survived the allocator stress test."
