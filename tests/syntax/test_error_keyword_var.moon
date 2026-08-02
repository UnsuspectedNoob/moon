##
  test_error_keyword_var.moon
  Ensures reserved keywords cannot be used inside spaced variable names
##

let player is cool be true

# expect error: You cannot use reserved keywords (like 'and', 'or', 'is') inside spaced variable names.
