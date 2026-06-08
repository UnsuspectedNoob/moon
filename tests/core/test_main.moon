load "test_mod.moon" as mymod

# Access the variable
show mymod's my_var

# Try to access the function as a value
let f be mymod's my_func
f
# expect error: Could not open file "test_mod.moon".
