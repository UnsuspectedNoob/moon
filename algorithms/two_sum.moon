# ---------------------------------------------------------
# TWO SUM
# Showcases Moon's Dictionary data structure and 
# its intuitive subscript assignment syntax.
# ---------------------------------------------------------
let two_sum (nums, target):
  let seen be {
  }

  for i from 1 to nums's length:
    let num be nums[i]
    let diff be target - num

    # Check if the difference is already in the dictionary
    if seen[diff] is not nil:
      return [ seen[diff], i ]
    end

    # Store the index of the current number using its value as the key
    set seen[num] to i
  end

  return [ ]
end

show "--- Two Sum Algorithm ---"
let numbers be [ 5 to 13, 15 to 30 ]
let target be 30

show"
Finding pair that sums to `target`
in `numbers`:"
let result be two_sum(numbers, target)

show"Indices: `result`"
for i in result
  show"at position `i` is `numbers.i`"

