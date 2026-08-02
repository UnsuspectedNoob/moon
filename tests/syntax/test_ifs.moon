##
  test_ifs.moon
  Antigravity Engine Test: Control Flow Grammar Validation
##

let score be 85
let rank be "B"

show "--- TEST 1: Single Statement (No Else) ---"
# expect: --- TEST 1: Single Statement (No Else) ---
# Grammar: 'if' expr single_statement
if score > 50 show "Test 1 Passed"
  # expect: Test 1 Passed

show "\n--- TEST 2: Standard Block ---"
# expect: 
# expect: --- TEST 2: Standard Block ---
# Grammar: 'if' expr ':' statements 'end'
if score > 80:
  show "Test 2 Passed"
  set rank to "A"
end
# expect: Test 2 Passed

show "\n--- TEST 3: Block-to-Single Else ---"
# expect: 
# expect: --- TEST 3: Block-to-Single Else ---
# Grammar: 'if' expr ':' statements 'else' single_statement
# The 'else' closes the block. The single statement requires no 'end'.
if score > 90:
  show "Test 3 Failed (Should not print)"
else
  show "Test 3 Passed"
# expect: Test 3 Passed

show "\n--- TEST 4: Single-to-Block Else ---"
# expect: 
# expect: --- TEST 4: Single-to-Block Else ---
# Grammar: 'if' expr single_statement 'else' ':' statements 'end'
if score < 50 show "Test 4 Failed"
else:
  show "Test 4 Passed"
  set score to 90
end
# expect: Test 4 Passed

show "\n--- TEST 5: The Antigravity Chain ---"
# expect: 
# expect: --- TEST 5: The Antigravity Chain ---
# Grammar: Block -> Else If (Single) -> Else (Block)
if score == 100:
  show "Test 5 Failed (Score is 90)"
else if score == 90 show "Test 5 Passed (Hit the single statement!)"
else:
  show "Test 5 Failed (Fell through)"
end
# expect: Test 5 Passed (Hit the single statement!)

show "\n--- TEST 6: Nested Ambiguity Check ---"
# expect: 
# expect: --- TEST 6: Nested Ambiguity Check ---
# Ensures the parser correctly attaches the 'else' to the closest 'if'
if rank == "A":
  if score == 90 show "Test 6 Passed"
    else show "Test 6 Failed (Inner Else)"
else
  show "Test 6 Failed (Outer Else)"
# expect: Test 6 Passed
