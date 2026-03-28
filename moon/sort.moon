
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

let sorted (list):
  if list's length < 2 give list

  let left be list[1 to end / 2]
  let right be list[end / 2 + 1 to end]

  give merge (sorted left) with (sorted right)
end

show sorted [10 to 1]
