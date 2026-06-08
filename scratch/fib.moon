let memo be {
}

let fib (n: Number):
  if n < 2:
    give n
  end

  if memo[n] is not nil:
    give memo[n]
  end

  let result be (fib n - 1) + (fib n - 2)
  set memo[n] to result
  give result
end

show fib 2000
