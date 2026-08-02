##
  test_shift.moon
  Tests the 'shift from <list>' standard library function with merge sort
##

let merge (left: List) with (right: List):
  let result be []

  while left's length > 0 and right's length > 0:
    add (shift from left) if left[1] < right[1] else (shift from right) to result
  end

  add left to result if left's length > 0
  add right to result if right's length > 0

  give result
end

let a sorted (list: List):
  give list if list's length < 2

  let mid be list's length / 2
  let left be list[1 to mid]
  let right be list[mid + 1 to end]

  give merge (a sorted left) with (a sorted right)
end

let items be [5, 1, 3, 2, 7, 6, 15, 8, 4, 10, 23, 12]
show "Listed:   `items`"
# expect: Listed:   [5, 1, 3, 2, 7, 6, 15, 8, 4, 10, 23, 12]

show "Sorted:   `a sorted items`"
# expect: Sorted:   [1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 15, 23]
