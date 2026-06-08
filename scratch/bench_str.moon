# Benchmark: Massive string concatenation
let str be ""
for i in 1 to 500000:
  update str + "a"
end
# Moon uses Ropes, so the concatenation itself is fast!
# We won't print it to keep the test purely on concatenation logic.
