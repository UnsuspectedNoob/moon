show "--- HASH TABLE BENCHMARK START ---"

let start_time be clock()

@@ 1. INSERTION STRESS
@@ We force the table to constantly dynamically resize and handle collisions
let map be {}
for i in 1 to 100000:
  let key be "k_`i`"
  set map[key] to i
end

let mid_time be clock()

@@ 2. LOOKUP STRESS
@@ We force the table to probe for existing keys thousands of times
let sum be 0
for i in 1 to 100000:
    let key be "k_`i`"
    add map[key] to sum
end

let end_time be clock()

show "1. Insertion Time (Seconds):"
show mid_time - start_time

show "2. Lookup Time (Seconds):"
show end_time - mid_time

show "3. Total Execution Time:"
show end_time - start_time

show "Verification Sum (Should be 5000050000): `sum`"

show "--- BENCHMARK END ---"
