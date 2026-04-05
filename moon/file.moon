
type Player:
  name: "Munachi"
end

let sum of (l : Range or List or String):
  let total be 0
  for each i in l add i as Number to total
  give total
end


show sum of 1 to 100
show sum of [1, 2, 3]
show sum of "-3433"
