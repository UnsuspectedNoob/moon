

let primes up to (n: Number):
  let prime be [for i in 1 to n keep true]
  set prime[1] to false
 
  let p be 2
  until p * p > n:
    if prime[p]
      for i in p * p to n by p
        set prime[i] to false

    update p + 1
  end

  let result be []
  for i from 1 to n
    if prime[i] is true
      add i to result

  give result
end

show primes up to 30000
