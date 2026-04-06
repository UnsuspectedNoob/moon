
let primes up to (n: Number):
  let prime be [for i in 1 to n keep true]
  set prime[1] to false
 
  let p be 2
  while p * p <= n:
    if prime[p]:
      for i in p * p to n by p:
        set prime[i] to false
      end
    end

    update p + 1
  end

  let result be []
  for i from 1 to n
    if prime[i] is true
      add i to result

  give result
end

show "--- STRING CONCATENATION BENCHMARK ---"

let text be ""
let start_time be clock

for each i in 1 to 1600000:
    set text to text + "x"
end

let end_time be clock

show "Built string of length: " + text's length
show "Time taken: " + (end_time - start_time) + " seconds."
