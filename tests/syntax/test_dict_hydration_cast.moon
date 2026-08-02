type Spaceship:
  fuel is 100,
  name is "Apollo"
end

# Cast a raw dictionary into a Blueprint instance using 'as'
let ship be {
  "fuel": 50,
  "name": "Gemini"
} as Spaceship

show ship's name
# expect: Gemini

show ship's fuel
# expect: 50

# Strict hydration failure check
let bad_ship be {
  "lasers": 2
} as Spaceship
# expect error: Dictionary contains a key not present on the Blueprint.
