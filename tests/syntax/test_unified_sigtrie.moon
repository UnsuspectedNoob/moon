# Test 1: Basic Declarations and Name Mangling
let player score be 100
show player score
# expect: 100

# Test 2: Mutation Statements (set, add, update)
let current health be 100
set current health to 50
add 25 to current health
update current health - 10
show current health
# expect: 65

# Test 3: Unified Properties and Fields
type Player:
  first name is "Munachi",
  last name is "Ukpai",
  max health is 30,

  full name:
  give "`my first name` `my last name`"
end
end

let p be Player {
  max health: 100
}
show p's max health
# expect: 100

set p's max health to 200
show p's max health
# expect: 200

show p's full name
# expect: Munachi Ukpai

# Test 4: Spaced Instantiation Keys
let p2 be Player {
  max health: 150,
  first name: "Aegus"
}
show p2's max health
# expect: 150
show p2's first name
# expect: Aegus
