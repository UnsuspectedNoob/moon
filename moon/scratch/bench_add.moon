# Benchmark: Merging massive lists
let a be [ 1 to 500000 ]
let b be [ 500001 to 1000000 ]

# OP_ADD (List Cloning)
let c be a + b

# OP_ADD_INPLACE (List Appending)
add b to a
