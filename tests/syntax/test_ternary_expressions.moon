##
  test_ternary_expressions.moon
  Tests for ternary expressions (<expr> if <cond> else <expr>) vs statement modifiers (<stmt> if <cond>)
##

# 1. Simple Ternary Assignment
let is_vip be true
let discount be 20 if is_vip else 0
show "Discount: `discount`"
# expect: Discount: 20

set is_vip to false
set discount to 20 if is_vip else 0
show "Discount: `discount`"
# expect: Discount: 0

# 2. Ternary in Math Expressions
let base_price be 100
let final_price be base_price - (15 if not is_vip else 30)
show "Final price: `final_price`"
# expect: Final price: 85

# 3. Ternary with Strings
let status_code be 200
let message be "Success" if status_code == 200 else "Error"
show "Status: `message`"
# expect: Status: Success

# 4. Statement Modifier (Single Statement with Trailing If)
let logged_in be true
show "User is logged in" if logged_in
# expect: User is logged in

set logged_in to false
show "This should not appear" if logged_in

# 5. Statement Modifier with Unless
show "Access granted" unless logged_in
# expect: Access granted
