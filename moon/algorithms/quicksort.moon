
# ---------------------------------------------------------
# QUICKSORT
# Showcases Moon's powerful list slicing, list concatenation,
# list comprehensions, and implicit 'it' iterators.
# ---------------------------------------------------------
let quicksort(list):
    # Base case
    if list's length <= 1:
        return list
    end

    # Better pivot selection to avoid O(N^2) on reverse-sorted lists
    let pivot be list[end/2]
    
    # Utilize list comprehensions with inline conditionals
    # to dynamically filter the arrays into three partitions!
    let left be [for each x in list keep x if x < pivot]
    let middle be [for each x in list keep x if x is pivot]
    let right be [for each x in list keep x if x > pivot]
    
    # Recursively sort and easily concatenate lists
    return quicksort(left) + middle + quicksort(right)
end

show "--- Quicksort Algorithm ---"

let numbers be [1000 to 1]
show "Original List:"
show numbers

let sorted be quicksort(numbers)
show "\nSorted List:"
show sorted
