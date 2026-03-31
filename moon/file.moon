type Spell:
  pattern: "Basic Weave",
  cost: 10
end

type Mage:
  name,
  essencePool: 3000,
  spells: []
end

let channel (spell: Spell) into (mage: Mage):
  if mage's essencePool < spell's cost:
    show "`mage's name` does not have enough essence to cast `spell's pattern`!"
    give nil
  end

  add -spell's cost to mage's essencePool

  show "`mage's name` successfully weaves `spell's pattern`!"
  show "Remaining Essence Pool: ` mage's essencePool `"
end

let channel (spellName: String) into (mage: Mage):
  for each spell in mage's spells:
    if spell's pattern = spellName:
      channel spell into mage
      quit
    end
    
    show "not looking for `spell's pattern`"
  end
end

let caster be Mage with
  name: "Archmage Munachi",
  spells: [
    Spell {},
    Spell { pattern: "Void Collapse", cost: 400 },
    Spell { pattern: "Inferno Pattern", cost: 30 },
    Spell { pattern: "Water", cost: 32 },
  ]
end

channel "Void Collapse" into caster
channel "Water" into caster
channel "Void Collapse" into caster
