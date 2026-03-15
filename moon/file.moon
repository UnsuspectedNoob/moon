let the factorial of (n):
  let result be 1
  for each i from 2 to n:
    set result to result * i
  end

  return result
end

let max of (a) and (b):
  if a > b
    return a
  else
    return b
end

show max of 150 and the factorial of 50
