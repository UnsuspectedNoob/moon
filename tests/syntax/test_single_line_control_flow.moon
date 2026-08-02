##
  test_single_line_control_flow.moon
  Tests for single-statement control flow (if, unless, while, until)
##

let score be 75
let active be true

# Single statement if
if score > 50 show "Score is high"
# expect: Score is high

if score < 50 show "Score is low"

# Single statement unless
unless score < 50 show "Unless passed"
# expect: Unless passed

# Single statement if-else
if score == 75 show "Score matched 75"
else show "Score did not match"
# expect: Score matched 75

if score == 100 show "Branch A"
else show "Branch B"
# expect: Branch B

# Single statement in loops
let counter be 0
while counter < 3 set counter to counter + 1
show "Final counter: `counter`"
# expect: Final counter: 3

let num be 5
until num == 2 set num to num - 1
show "Final num: `num`"
# expect: Final num: 2
