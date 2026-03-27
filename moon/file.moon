
let get odds up to (limit):
  let odds be []

  for each i from 1 to limit
    add i to odds if i mod 2 is not 0

  give odds
end

show get odds up to 5000000

