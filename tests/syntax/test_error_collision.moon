##
  test_error_collision.moon
  Ensures collision between 0-arg function and variable is detected
##

let player name be "Bob"
let player name:
  give "Alice"
end

# expect error: A variable with this exact name already exists.
