let x be 10

if x > 5:
  show "Greater!"
else:
  show "Smaller!"
end

unless x == 10:
  show "Not ten!"
then:
  show "It is ten!"
end

let y be "Fast" if x > 5 else "Slow"
show y

# expect: Greater!
# expect: It is ten!
# expect: Fast
