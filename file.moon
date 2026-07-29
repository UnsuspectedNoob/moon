
let change (x) to (y):
  show "`x` is changing to `y`"
end

let change (x: Range):
  show "`x` is now a range"
end

change 5 to 6
change (5 to 29)
