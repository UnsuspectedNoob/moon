# tester.moon
load "lib_stress.moon" as s

# 1. Test Namespaced Access
show "Module Version: " + s's get version
show s's name
show "Accessing namespace..."
let p be s's Player { name: "Munachi", health: 250 }
show "Player name: " + p's name

# 2. Test Type Hydration (Dict -> Blueprint)
let data be { "name": "Emrys", "health": 999 }
let hydrated be data as s's Player
show "Hydrated Health: " + (hydrated's health as String)

# 3. Test Euclidean Modulo
show "Modulo test (-5 mod 3 should be 1): " + ((-5 mod 3) as String)

# 4. Test Floating Point Epsilon
let float_sum be 0.1 + 0.2
show "Floating point (0.1 + 0.2 == 0.3) should be true: " + ((float_sum is 0.3) as String)

# 5. Test Union Type Dispatch
let process (v: String or Number):
  show "Dispatching Union: " + (v as String)
end

process "Moonlight"
process 150
