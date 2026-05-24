import "core" as core

# ---------------------------------------------------------
# QUICKSORT
# Showcases Moon's powerful list slicing, list concatenation,
# list comprehensions, and implicit 'it' iterators.
# ---------------------------------------------------------
let quicksort(list):
    # Base case
    if list.length <= 1:
        return list
    end

    # The pivot is the first element
    let pivot be list[0]
    
    # Slice the rest of the list seamlessly using 'end' index
    let rest be list[1 to end]
    
    # Utilize list comprehensions with inline conditionals
    # to dynamically filter the arrays!
    let left be [keep if it <= pivot for it in rest]
    let right be [keep if it > pivot for it in rest]
    
    # Recursively sort and easily concatenate lists
    return quicksort(left) + [pivot] + quicksort(right)
end

core.print("--- Quicksort Algorithm ---")

let numbers be [34, 7, 23, 32, 5, 62, 32, 1, 9, 12, 8]
core.print("Original List:")
core.print(numbers)

let sorted be quicksort(numbers)
core.print("\nSorted List:")
core.print(sorted)
