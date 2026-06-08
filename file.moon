
let max of (a: Number) and (b: Number):
  give a if a > b else b
end

let max of (a: Number, b: Number) and (c: Number):
  give max of a and max of b and c
end

let max of (a: String) and (b: Number):
  give max of a as Number and b
end

show max of 3 and 30
show max of (3, max of 30 and 40) and 23
show max of "350" and 3
