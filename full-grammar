
### I. Lexical Grammar (The Tokens)
This defines how raw text characters are grouped into the basic vocabulary of MOON.

```ebnf
<identifier>   ::= [a-zA-Z_] [a-zA-Z0-9_]*
<number>       ::= [0-9]+ ( "." [0-9]+ )?
<string>       ::= '"' [^"]* '"'
<boolean>      ::= "true" | "false"
<nil>          ::= "nil"

<comment>      ::= "##" [^\n]*
```

**Keywords:**
`add`, `and`, `ask`, `be`, `by`, `break`, `each`, `else`, `end`, `false`, `for`, `from`, `give`, `if`, `in`, `is`, `let`, `nil`, `not`, `or`, `quit`, `return`, `set`, `skip`, `then`, `to`, `true`, `type`, `unless`, `until`, `update`, `while`, `with`

**Operators & Punctuation:**
`+`, `-`, `*`, `/`, `%`, `=`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `!`, `:`, `,`, `.`, `'s`, `(`, `)`, `[`, `]`, `{`, `}`, `\``

---



<program>       ::= <declaration>* EOF

<declaration>   ::= <type_decl> 
                  | <let_decl> 
                  | <statement>

<type_decl>     ::= "type" <identifier> ":" ( <property_def> ("," <property_def>)* )? "end"
<property_def>  ::= <identifier> ( "be" <expression> )?

<let_decl>      ::= "let" <identifier> ("," <identifier>)* "be" <expression> ("," <expression>)* <modifier>?
                  | "let" <phrasal_sig> ":" <block> "end"

<phrasal_sig>   ::= ( <identifier> | "(" <identifier> ":" <identifier> ")" )+

<statement>     ::= <if_stmt> 
                  | <unless_stmt> 
                  | <while_stmt> 
                  | <for_stmt> 
                  | <action_stmt>

<block>         ::= <declaration>*

<if_stmt>       ::= "if" <expression> ":" <block> ( "else" "if" <expression> ":" <block> )* ( "else" ":" <block> )? "end"
<unless_stmt>   ::= "unless" <expression> ":" <block> ( "then" <block> )? "end"
<while_stmt>    ::= ( "while" | "until" ) <expression> ":" <block> "end"
<for_stmt>      ::= "for" "each"? <identifier> ( "in" | "from" ) <expression> ":" <block> "end"

<action_stmt>   ::= ( <break_stmt> 
                  | <skip_stmt> 
                  | <set_stmt> 
                  | <add_stmt> 
                  | <update_stmt> 
                  | <give_stmt> 
                  | <expr_stmt> ) <modifier>?

<break_stmt>    ::= "break" | "quit"
<skip_stmt>     ::= "skip"
<set_stmt>      ::= "set" <lvalue> ("," <lvalue>)* "to" <expression> ("," <expression>)*
<add_stmt>      ::= "add" <expression> ("," <expression>)* "to" <lvalue>
<update_stmt>   ::= "update" <lvalue> ( "+" | "-" | "*" | "/" | "%" ) <expression>
<give_stmt>     ::= ( "give" | "return" ) <expression>?
<expr_stmt>     ::= <expression>

<modifier>      ::= ( "if" | "unless" ) <expression>

<expression>    ::= <ternary>

<ternary>       ::= <logic_or> ( "if" <expression> "else" <expression> | "unless" <expression> "then" <expression> )?
<logic_or>      ::= <logic_and> ( "or" <logic_and> )*
<logic_and>     ::= <equality> ( "and" <equality> )*
<equality>      ::= <comparison> ( ( "is" | "is" "not" | "==" | "!=" | "=" ) <comparison> )*
<comparison>    ::= <term> ( ( "<" | ">" | "<=" | ">=" ) <term> )*
<term>          ::= <factor> ( ( "+" | "-" ) <factor> )*
<factor>        ::= <unary> ( ( "*" | "/" | "%" ) <unary> )*
<unary>         ::= ( "-" | "not" ) <unary> 
                  | <range>

<range>         ::= <call> ( "to" <call> ( "by" <call> )? )*

<call>          ::= <primary> ( "(" <arguments>? ")"
                              | "[" <expression> "]"
                              | "." <expression>
                              | "'s" <identifier>
                              | <instantiation> )*

<arguments>     ::= <expression> ( "," <expression> )*

<instantiation> ::= "{" ( <identifier> ":" <expression> ) ("," <identifier> ":" <expression> )* "}"
                  | "with" ( <identifier> ":" <expression> ) ("," <identifier> ":" <expression> )* "end"

#### 4. Primary Values & Memory Targets
The base values of the language and how memory addresses are targeted.

<primary>       ::= <number>
                  | <string>
                  | <boolean>
                  | <nil>
                  | "end"
                  | <identifier>
                  | <list_literal>
                  | <dict_literal>
                  | <interpolation>
                  | <phrasal_call>
                  | "(" <expression> ")"

<list_literal>  ::= "[" <expression> ( "," <expression> )* "]"
<dict_literal>  ::= "{" ( (<identifier> | <string>) ":" <expression> ) ("," (<identifier> | <string>) ":" <expression>)* "}"
<interpolation> ::= '"' [^`]* '`' <expression> ( '`' [^`]* '`' <expression> )* '`' [^"]* '"'

<phrasal_call>  ::= ( <identifier> | <expression> )+ 

<lvalue>        ::= <identifier> ( "[" <expression> "]" | "." <expression> | "'s" <identifier> )*

