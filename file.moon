
type Player:
  name,
  age,
  health: 100,
end

let (p: Player)'s info:
  give "Name: `p's name`, Age: `p's age`"
end

let player be Player {
  name: "Munachi",
  age: 30
}
show player's info
