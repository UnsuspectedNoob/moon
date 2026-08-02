##
  test_error_operator_shadow.moon
  Ensures operator phrases without custom identifier anchors are rejected
##

let (a) and (b) or (c):
  give a
end

# expect error: Operator phrase requires a custom identifier anchor.
