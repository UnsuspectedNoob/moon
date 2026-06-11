type Player:
  name: "Emrys"
end

let describe (entity: Player)'s status:
  show "Player is " + entity's name
end

let p be Player {}
describe p's status
