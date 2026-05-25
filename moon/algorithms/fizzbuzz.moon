# ---------------------------------------------------------
# FIZZBUZZ
# Showcases Moon's elegant loops, modulo operators, and
# condition chaining.
# ---------------------------------------------------------
let fizzbuzz(n):
    for i from 1 to n by 1:
        if i mod 15 is 0:
            show "FizzBuzz"
        else if i mod 3 is 0:
            show "Fizz"
        else if i mod 5 is 0:
            show "Buzz"
        else:
            show i
        end
    end
end

show "--- FizzBuzz Algorithm ---"
fizzbuzz(15)
