let max sum in (list) with (k) items:
  let n be list's length

  if k < 0 or > n:
    show "invalid number of items"
    give nil
  end

  let currentSum be 0
  for each number in list[1 to k]
    add number to currentSum

  give currentSum if n = k

  let maxSum be currentSum
  for each i from 1 to n - k:
    add list[i + k] - list.i to currentSum
    set maxSum to currentSum if currentSum > maxSum
  end

  give maxSum
end

show max sum in [2 to 100 by 2] with 2 items
