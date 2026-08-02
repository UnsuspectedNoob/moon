let p be {
  age: 10,
  name: "Munachi",
}

let (l: List)'s left:
  give l[1 to end / 2]
end

let (l: List)'s right:
  give l[end / 2 + 1 to end]
end

let list be [ 1 to 9 ]
show "`list's left` and `list's right`"
