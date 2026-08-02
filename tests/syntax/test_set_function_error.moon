let greet (name: String):
  show "Hello " + name
end

set greet to "Overwritten"

# expect error: Functions cannot be overwritten with 'set'.
