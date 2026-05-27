let quicksort (arr: List):
  if arr's length <= 1 give arr
  
  let pivot be arr[arr's length]
  let left, right be []
  
  for i from 1 to arr's length - 1:
    let val be arr[i]
    add val to left if val < pivot
    add val to right if val >= pivot
  end
  
  let result be quicksort left
  add pivot, quicksort right to result
  give result
end

let data be []
for i from 10000 to 1:
  add i to data
end

let sorted be quicksort data
show sorted's length
