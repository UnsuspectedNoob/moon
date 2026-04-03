show "--- INITIALIZING MAGIC SYSTEM ---"

type Mage:
  name: "Unknown",
  essencePool: 100
end

type Spell:
  pattern: "Basic Weave",
  cost: 10
end

## 2. Phrasal Functions with Multiple Dispatch
## The fallback method for generic inputs
let channel (a) into (b):
    show "Channeling raw energy..."
end

## The strict-typed method for our Magic System
let channel (spell: Spell) into (mage: Mage):
    if mage's essencePool < spell's cost:
      show "`mage's name` does not have enough essence!"
      give nil
    end

    add -spell's cost to mage's essencePool

    ## Testing string interpolation and possessive access!
    show "` mage's name ` successfully weaves the  ` spell's pattern `!"
    show "Remaining Essence Pool: ` mage's essencePool `"
end

## 3. Boot up the instances
let caster be Mage with
  name: "Archmage Munachi",
  essencePool: 50
end

let fireWeave be Spell with
  pattern: "Inferno Pattern",
  cost: 30
end

let voidWeave be Spell with
  pattern: "Void Collapse",
  cost: 40
end

## 4. Execute the system
show "--- CASTING FIRST SPELL ---"
channel fireWeave into caster

show ""
show "--- ATTEMPTING OVERDRAW ---"
channel voidWeave into caster
