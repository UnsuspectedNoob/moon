type Item:
end

let process (val: Number):
  give val
end

let process (val: String):
  give val
end

let process (val: Item):
  give val
end

let process (val: Any):
  give val
end

let start be clock
for each i in 1 to 10000000:
  process 10
end
let endTime be clock
show endTime - start
