
# ---------------------------------------------------------
# RECURSIVE FIBONACCI
# Showcases Moon's elegant phrasal functions, conditional 
# returns, and recursion capability.
# ---------------------------------------------------------
let fib_recursive(n):
    if n <= 1:
        return n
    end
    return fib_recursive(n - 1) + fib_recursive(n - 2)
end

# ---------------------------------------------------------
# ITERATIVE FIBONACCI
# Showcases Moon's action statements, fast VM math, 
# and robust 'for' loops.
# ---------------------------------------------------------
let fib_iterative(n):
    if n <= 1:
        return n
    end

    let a be 0
    let b be 1
    
    for i from 2 to n by 1:
        let temp be a + b
        set a to b
        set b to temp
    end
    
    return b
end

show "--- Fibonacci Algorithms ---"

show "Iterative (n=50):"
show fib_iterative(50)

show "\nRecursive (n=20):"
show fib_recursive(20)
