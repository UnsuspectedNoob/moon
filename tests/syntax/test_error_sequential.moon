##
  test_error_sequential.moon
  Ensures consecutive argument definitions without phrase words or commas are rejected
##

let (a)(b):
  give a + b
end

# expect error: Sequential arguments are forbidden in signatures.
