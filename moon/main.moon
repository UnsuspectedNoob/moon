# main.moon

# 1. Test the Module Loader and Scope Isolation
load "utils.moon" as u

show "Loaded Utils Version: " + (u's version as String)
u's greet ("Munachi")

# 2. Test Blueprint Instantiation across Modules
let my_point be u's Point { x: 10, y: 20 }
show "Point X is: " + (my_point's x as String)

# 3. Test the Phase 3 Math Fixes
let float_math be 0.1 + 0.2
show "0.1 + 0.2 == 0.3 is " + ((float_math is 0.3) as String)

let modulo_math be -5 mod 3
show "-5 mod 3 is " + (modulo_math as String)
