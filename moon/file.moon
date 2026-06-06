
type Player:
  name,
  age,
  address
end

let make player:
  give Player {
    name: "Munachi",
    age: 30,
    address: "22 Saji Ayangade street, Anthony Village, Lagos"
  }
end

show make player as Dict
