
# ---------------------------------------------------------
# BINARY SEARCH
# Showcases Moon's while loops, chained comparisons, 
# action assignment statements (update), and math library.
# ---------------------------------------------------------
let binarySearch (list, target):
  let low be 1
  let high be list's length

  while low <= high:
    # Calculate the midpoint and floor it using the math module
    let mid be floor of((low + high) / 2)
    let guess be list[mid]

    if guess == target:
      return mid
    else if guess > target:
      set high to mid - 1
    else:
      set low to mid + 1
    end
  end

  # Not found
  return - 1
end

show "--- Binary Search Algorithm ---"

let primes be [ 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97 ]

show "Searching for 43 in Primes List:"
let index be binarySearch(primes, 43)

if index is not - 1:
  show "Found at index:"
  show index
else:
  show "Target not found in list."
end
