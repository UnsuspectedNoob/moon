# ---------------------------------------------------------
# TWO SUM
# Showcases Moon's Dictionary data structure and 
# its intuitive subscript assignment syntax.
# ---------------------------------------------------------
let two_sum(nums, target):
    let seen be {}
    
    for i from 1 to nums's length by 1:
        let num be nums[i]
        let diff be target - num
        
        # Check if the difference is already in the dictionary
        if seen[diff] is not nil:
            return [seen[diff], i]
        end
        
        # Store the index of the current number using its value as the key
        set seen[num] to i
    end
    
    return []
end

show "--- Two Sum Algorithm ---"
let numbers be [100 to 5]
let target be 30

show "Finding pair that sums to 9 in [2, 7, 11, 15]:"
let result be two_sum(numbers, target)

show "Indices:"
show result
