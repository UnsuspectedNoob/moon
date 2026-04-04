
let merge (left: List) with (right: List):
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

let sorted (list: List):
  if list's length < 2 give list

  let left be list[1 to end / 2]
  let right be list[end / 2 + 1 to end]

  give merge (sorted left) with (sorted right)
end

let quick sort of (list: List):
  if list's length < 2 give list

  let pivot be list.1
  let rest be list[2 to end]

  let less, greater be []
  for each item in rest:
    add item to less if item <= pivot
    add item to greater if item > pivot
  end

  let result be quick sort of less
  add pivot, quick sort of greater to result

  give result
end

let a be [1000000 to 1]

show sorted a
