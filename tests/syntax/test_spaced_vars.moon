##
  test_spaced_vars.moon
  Tests multi-word spaced variables and phrasal setter functions
##

let player_score be 0

let player score:
  give player_score
end

let change player score to (n: Number):
  set player_score to n
end

show "Initial score: `player score`"
# expect: Initial score: 0

change player score to 50
show "New score: `player score`"
# expect: New score: 50
