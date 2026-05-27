# ---------------------------------------------------------
# PALINDROME
# Showcases Moon's elegant phrasal standard library:
# split, reverse, and join.
# ---------------------------------------------------------
let isPalindrome (word):
  # Standard library functions are deeply integrated as phrases!
  let chars be split word by ""
  let rev_chars be reverse chars
  let rev_word be join rev_chars with ""

  return word is rev_word
end

show "--- Palindrome Algorithm ---"
show "Is 'racecar' a palindrome?"
show isPalindrome("racecar")

show "Is 'moon' a palindrome?"
show isPalindrome("moon")
