@@ Our global cache dictionary
let memo be {}

let fibonacci of (n):
  @@ Base cases
  if n < 2 give n

  @@ Cache hit! Return immediately.
  if memo.n is not nil:
    give memo.n
  end

  @@ Cache miss. Calculate recursively.
  let result be (fibonacci of n - 1) + (fibonacci of n - 2)

  @@ Save to the dictionary for next time
  set memo.n to result
  
  give result
end

show "The 60th Fibonacci number is `fibonacci of 60`"
