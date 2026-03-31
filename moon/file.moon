show "--- VM DISPATCH BENCHMARK START ---"

let start_time be clock()

let counter be 0

@@ A tight loop running 10 million times.
@@ We use standard assignment instead of 'add to' to force the VM to run 
@@ a longer sequence of opcodes per iteration: 
@@ OP_GET_LOCAL -> OP_CONSTANT -> OP_ADD -> OP_SET_LOCAL -> OP_LOOP
while counter < 10000000:
    set counter to counter + 1
end

let end_time be clock()

show "Total Execution Time (Seconds):"
show end_time - start_time

show "--- BENCHMARK END ---"
