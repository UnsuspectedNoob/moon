let describe (x: Number):
  show "Number: `x`"
end

let describe (x: String):
  show "String: `x`"
end

describe 42
# expect: Number: 42

describe "Moon"
# expect: String: Moon
