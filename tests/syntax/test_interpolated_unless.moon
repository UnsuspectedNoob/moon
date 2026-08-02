let status be "awake"

# The interpolation parser should properly handle the nested strings and logic
show "The creator is `status unless status == "asleep" then "dreaming"` right now."
  # expect: The creator is awake right now.
