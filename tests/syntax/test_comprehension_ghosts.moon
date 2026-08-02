let numbers be [ 1, 2, 3, 4, 5 ]

# Using a statement modifier inside the comprehension body
let evens be [ for each n in numbers keep n if n mod 2 == 0 ]

# Remember, 1-based indexing!
show evens[1]
# expect: 2

show evens[2]
# expect: 4
