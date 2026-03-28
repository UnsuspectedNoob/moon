
type Person:
  name,
  age: 20
  health: 100,
end

let person be new Person with:
  name: "Munachi"
end

let User type:
  name, age, balance
end

const user =  {
  name: "Munachi",
  age: 50,
  balance: 0
}

User {
  name: "Munachi",
  age: 30
}

type Book:
  name, author,
  ISBN = "1023-3443-5332-3418",
  borrowed = false
end
