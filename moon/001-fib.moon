

let first (n) fibonacci numbers:
  let a be [1, 1]

  give a if n < 3

  for i from 1 to n - 2
    add a[end] + a[end - 1] to a

  give a
end

show first 10 fibonacci numbers
