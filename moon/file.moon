let State be {
  player_name: "Munachi",
  health: 100
}

## The Getters
let player name: give State["player_name"] end
let player health: give State["health"] end
let player info: give "
Name: `player name`
Health: `player health`"

## The Setters
# A comment
# More comments

#: A multiline comment
   This comment continues until it sees a token
   on the same indent as the beginning #:

let heal player by (amount: Number):
  update State["health"] + amount
end

let rename player to (new_name: String):
  set State["player_name"] to new_name
end

rename player to "Himmel the Hero"
show player info
heal player by 3000
show player info
