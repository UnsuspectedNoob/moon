
let merge (left) with (right):
  let result be []
  let i, j be 1

  until i > left's length or j > right's length:
    if left.i < right.j:
      add left.i to result
      add 1 to i
    else:
      add right.j to result
      add 1 to j
    end
  end

  add left[i to end] to result
  add right[j to end] to result

  give result
end

let sort (list):
  if list's length < 2 give list

  let mid be list's length / 2
  let left be list[1 to mid]
  let right be list[mid + 1 to end]

  give merge (sort left) with (sort right)
end

let numbers be [38, 27, 43, 3, 9, 82, 10]

show "Unsorted: `numbers`"
show "Sorted:   `sort (numbers)`"
