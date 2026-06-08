type Person:
  name: "Munachi",
  age: 30
end

let show (person: Person):
  show "Name: `person's name`, Age: `person's age`"
end

let p be Person { }
show p
