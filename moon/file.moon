show "--- INITIATING GC STRESS TEST ---"

@@ 1. Define a custom Blueprint
type Particle:
  id,
  payload
end

@@ 2. Define a phrasal function to encapsulate scope
let process (n) times:
    let accumulator be []

    for i in 1 to n:
        @@ 3. Thrash the string intern pool and force concatenations
        let junkStr be "entropy_hash_`i`_`clock()`"

        @@ 4. Thrash the Dictionary allocator with the dynamic string
        let data be {
          "cycle": i,
          "signature": junkStr
        }

        @@ 5. Thrash the Instance allocator
        let p be Particle with
          id: i,
          payload: data
        end

        @@ 6. Force OP_ADD_INPLACE to trigger list array reallocations
        add p to accumulator

        @@ At the end of every loop iteration, 'junkStr', 'data', and 'p' 
        @@ fall out of local scope. But because 'p' is appended to the 
        @@ accumulator, it survives. The GC has to perfectly trace the 
        @@ surviving graph while incinerating the intermediate temporary strings.
    end

    give accumulator
end

let n be 150000
show "Allocating `n` nested structures..."

@@ Fire the engine. 15,000 iterations should comfortably push 
@@ vm.bytesAllocated way past the 1MB threshold, triggering multiple GC cycles.
let result be process n times

show "Stress test complete. Survived with item count:"
show result's count
