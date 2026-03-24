
let merge (left) with (right):
  let result be []
  let i, j be 1

  until i > left's length or j > right's length
    if left.i < right.j:
      add left.i to result
      add 1 to i
    else:
      add right.j to result
      add 1 to j
    end

  add left[i to end] to result
  add right[j to end] to result

  give result
end

let sort (list):
  if list's length < 2 give list

  let left be list[1 to end / 2]
  let right be list[end / 2 + 1 to end]

  give merge (sort left) with (sort right)
end


let quick sort of (list):
  if list's length < 2 give list

  let pivot be list[1]
  let rest be list[2 to end]

  let less, greater be []

  for each item in rest:
    if item < pivot add item to less else add item to greater
  end

  let result be quick sort of less
  add pivot, quick sort of greater to result

  give result
end

show quick sort of [1 to 50]



let list be [1 to 5]

let reverse (list):
  for each i from 1 to list's length / 2
    set list.i, list[-i] to list[-i], list[i]

  give list
end

show reverse list
