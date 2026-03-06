let x be 3
let x, y be 0 # both x and y are 0
let x, y be 80, 100

let greet:
  let name be "Munachi"
  give "Hello there, `name`"
end

let list be [5, 2, 3, 7, 1, 10, 8]
sort list
show list

let name be "Munachi"
set name to "`name` Ukpai"
add " Ukpai" to name
set name, x to "both are changed to this"
set a, b, c, d to "one", "after", "the", "other"

let i be 2
let list be [1, 2, 3, 4]
show list // [1, 2, 3, 4]
show list.1 // 1

show list.(i + 1) // 3
show list[1] // 1
show list[-1] // 4
show list[1 to 3] // [1, 2, 3]
show list[1 to end] // [1, 2, 3, 4]
show list's length // 4
show list[end] // 4

let type Stack:
  items be []
  length give items' length
  top give items[end]
end

set stack's top to 5

