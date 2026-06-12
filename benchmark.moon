type Player:
  name: "Emrys",
  health: 100
end

let p be Player {
}

let describe (entity: Player)'s status:
  give entity's health
end

let i be 0
let start be clock
while i < 100000:
  describe p's status
  update i + 1
end
let finish be clock

show "Executed 100,000 dispatches in: "
show finish - start
