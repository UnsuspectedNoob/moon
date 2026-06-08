
#i:      1
#left:  [2, 3, 5]

#j:      1
#right: [1, 4, 6, 8]
let merge (left: List) with (right: List):
let total be [ ]
let i, j be 1

let leftLen be left's length
let rightLen be right's length

until i > leftLen or j > rightLen:
  if left[i] < right[j]:
    add left[i] to total
    update i + 1
  else:
    add right[j] to total
    update j + 1
  end
end

add left[i to end] to total unless i > leftLen
add right[j to end] to total unless j > rightLen

give total
end

let sorted (list: List):
  if list's length < 2
    give list

  let left be list[1 to end / 2]
  let right be list[end / 2 + 1 to end]

  give merge (sorted left) with (sorted right)
end

let a be [ 1000000 to 1 ]
sorted a

