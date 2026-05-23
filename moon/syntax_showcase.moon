let printTitle (title):
  show ""
  show "--- `title` ---"
end

printTitle "1. Variables & Primitives"
let number be 42
let decimal be 3.14
let truth be true
let lies be false
let nothing be nil
let string be "Hello, Moon!"
let a, b be 1, 2

show number
show decimal
show truth
show string
show a

printTitle "2. String Interpolation"
let name be "Emrys"
let greeting be "Hello `name`! Welcome to Moon."
show greeting

printTitle "3. Collections & Access"
let list be [1, 2, 3, 4]
let user be { name: "Emrys", speed: "Fast" }

show list
show user
show user["name"]
show user's speed

printTitle "4. Math, Logic, and Ranges"
let sum be 10 + 5
let diff be 10 - 5
let prod be 10 * 5
let quot be 10 / 2
let remainder be 10 mod 3

let isTrue be true and false or not false
let isEqual be 10 is 10
let isNotEqual be 10 is not 5
let isGreater be 10 > 5

let myRange be 1 to 10 by 2

show sum
show isTrue
show isGreater
show myRange

printTitle "5. Custom Types & Instantiation"
type Player:
  name: "Unknown",
  health: 100
end

let p1 be Player with name: "Emrys" end
show p1's name
show p1's health

printTitle "6. Control Flow"
if 10 > 5:
  show "10 is indeed greater than 5"
end

unless false:
  show "Unless executes when false!"
end

let result be "Ternary" if true else "Fallback"
show result

printTitle "7. Action Statements & Modifiers"
let counter be 0
add 5 to counter
set counter to 10
update counter * 2

show counter if counter > 10
set counter to 0 unless counter is 0

printTitle "8. Loops"
let i be 0
while i < 3:
  add 1 to i
  show "While loop iteration"
end

let j be 0
until j is 3:
  add 1 to j
  show "Until loop iteration"
end

for count from 1 to 3 by 1:
  show "For loop iteration"
end

for each item in list:
  show "For each item"
  break if item is 2
end

printTitle "9. Comprehensions & The it Construct"
let numbers be [1, 2, 3, 4, 5]
let doubled be [for each n in numbers keep n * 2 if n > 2]

let users be [{name: "Alice", age: 25}, {name: "Bob", age: 17}]
let adults be [for each u in users keep u if it's age > 18]

show doubled
show adults

printTitle "10. Phrasal Functions"
let push (item) to (list):
  show "Pushing to list..."
end

let jump:
  show "Jumping!"
end

jump
push 10 to list

show "Showcase executed successfully!"
