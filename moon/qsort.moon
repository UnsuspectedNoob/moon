let quick sort of (list: List):
  if list's length < 2 give list

  let pivot be list.1
  let rest be list[2 to end]

  let less, greater be [ ]
  for each item in rest:
    add item to less if item <= pivot
    add item to greater if item > pivot
  end

  let result be quick sort of less
  add pivot, quick sort of greater to result

  give result
end

let a be [ 1000 to 1 ]
let sorted be quick sort of a
show sorted's length
