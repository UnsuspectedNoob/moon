# More about a Player ?
# a player file ?
type Player:
  name,
  age: 30,
  wealth,
  balance
end

let player be Player {
  name: |Aegus|,
  age: 30,
  wealth: 30000,
  balance: 30
}

show player

let books be [ 1 to 10 ]
show books[1 to end / 2]

let sum of (list: List or Range or Player):
  let total be 0
  for each i in list update total + i
  give total
end

# a List of jazz songs
let jazz songs:
  let total be 0
  give [ |Tenderly|, |Blue Moon|, |Fools Rush In|, |Just in Time| ]
end

show jazz songs
show player's name
