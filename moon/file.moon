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
let heal player by (amount: Number):
  update State["health"] + amount
end

let rename player to (new_name: String):
  set State["player_name"] to new_name
end

show player name
rename player to "Himmel"
show player name
show player info
heal player by 3000
show player info
