

let primes up to (n: Number):
  let prime be [ for i in 1 to n keep true ]
  set prime[1] to false

  let p be 2
  until p * p > n:
    if prime[p]:
      for i in p * p to n by p
      set prime[i] to false
    end

    update p + 1
  end

  let result be [ ]
  for i from 1 to n
    if prime[i] is true
      add i to result

  return result
end

let x, y be 90, 100
let a be primes up to x
let b be primes up to y
show a
show b
show b's length - a's length

