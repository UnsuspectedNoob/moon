

let the first (n: Number) fibonacci numbers:
  let a be [ 0, 1 ]

  give a if n < 3

  for i from 1 to n - 2:
    add a[end] + a[end - 1] to a
  end

  give a
end

show the first 10 fibonacci numbers
