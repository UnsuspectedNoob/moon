

type Player:
  name, age
end

let player be Player {
  name: "munachi@dd",
  age: 22
}

let info:
  show "Name: `player's name`
Age: `player's age`"
end

let increase age of (player: Player) by (n: Sting):
  update player's age + n
end

increase age of player by "1" as Number
info
