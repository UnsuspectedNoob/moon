show "Testing Embedded Methods..."

type Player:
  name, health,
  my info:
    give "`my name` has `my health` health."
  end
end

let p be Player with
  name: "Emrys",
  health: 100
end

show p's info
