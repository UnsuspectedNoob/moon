# Top-level variable
let count be 1
set count to count + 1
show count
# expect: 2

# Local variable & shadowing
let run:
  let a be 10
  if true:
    let a be 20
    set a to 25
    show a
  end
  show a
end
run
# expect: 25
# expect: 10

# Object property
type Car:
  speed is 0
end
let c be Car with speed: 50
end
set c's speed to 100
show c's speed
# expect: 100

# Subscript
let list be [10, 20, 30]
set list[1] to 99
show list[1]
# expect: 99
