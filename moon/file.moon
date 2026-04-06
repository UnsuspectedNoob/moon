
let x be ""

let start_time be clock
for i in 1 to 10000000
  set x to x + "x"
let end_time be clock

show "Length of x: `x's length`"
show "Time: `end_time - start_time`"
