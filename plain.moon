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
set a, b, c, d to "a", "b", "c", "d"

let i be 2
let list be [10, 20, 30, 40]
show list # [10, 20, 30, 40]
show list.1 # 10
show list.i # 20

show list.(i + 1) # 30
show list[i + 1] # 30
show list[1] # 10
show list[-1] # 40
show list[1 to 3] # [10, 20, 30]
show list[1 to end] # [10, 20, 30, 40]
show list's length # 4
show list[end] # 40

