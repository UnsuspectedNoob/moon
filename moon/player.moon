## player.moon - An OOP-style module using Global Verbs

type Player:
  name: "Unknown",
  health: 100
end

let heal (target: Player) by (amount: Number):
  update target's health + amount
  show target's name + " was healed for " + amount + " HP!"
end

let take (damage: Number) damage on (target: Player):
  update target's health - damage
  show target's name + " took " + damage + " damage!"
end
