
let name be "Munachi"
show first letter of name

let delete (item) from (list):
  remove item from list
end

func-decl ::= "let" ( <id> <label>* "(" <args> ")" )+ <block>
<args> ::= <id> ( , <id> )*
<block> ::= ":" <statements> "end"
<statements> ::= <statement> <statement>*

let max of (a, b):
  give a if a > b else b
end

let first letter of (name):
  show name.1
end
