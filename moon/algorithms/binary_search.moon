import "core" as core
import "math" as math

# ---------------------------------------------------------
# BINARY SEARCH
# Showcases Moon's while loops, chained comparisons, 
# action assignment statements (update), and math library.
# ---------------------------------------------------------
let binarySearch(list, target):
    let low be 0
    let high be list.length - 1

    while low <= high:
        # Calculate the midpoint and floor it using the math module
        let mid be math.floor((low + high) / 2)
        let guess be list[mid]

        if guess == target:
            return mid
        else if guess > target:
            update high as mid - 1
        else:
            update low as mid + 1
        end
    end

    # Not found
    return -1
end

core.print("--- Binary Search Algorithm ---")

let primes be [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97]

core.print("Searching for 43 in Primes List:")
let index be binarySearch(primes, 43)

if index != -1:
    core.print("Found at index:")
    core.print(index)
else:
    core.print("Target not found in list.")
end
