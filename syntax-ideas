
let excos be ["Tonas", "Seyi", "Chris"]
let lastNames be ["Onaola", "Shay", "Tucker"]

for each exco in excos, name in lastNames
  show "`exco` `name`"

let books be [
  Book("The Power of Habit", "Charles Duhigg"),
  Book("Atomic Habits", "James Clear"),
  { name: "Pride and Prejudice", "Jane Austen" },
  { "Pride and Prejudice", "William Atkinson" }
]

books.4.2

let p be {1, 2}
let q be {x= 1, y= 2}
show p.1
show q.x

let buy take (book) => "buys `book`"
let max be (a, b) => a (unless b > a then b)
let max be (a, b) => a (if a > b else b)
let max be (a, b):
  if a > b give a
  else give b
end

let sort (list):
  if list's length <= 1 return list

  let pivot be list[end / 2]
  let left, right be []

  for each num in list
    if num <= pivot
      add num to left
    else
      add num to right

  give [ sort(left), mid, sort(right) ]
end


for each book in books buy(book)

let x be 50
set x to 5000

let mergeSort be (list):
end

let merge take (left, right):
  let result be []
  let i, j be 1

  until i > left's length or j > right's length
    if left.i <= right.j:
      add left.i to result
      add 1 to i
    else:
      add right.j to result
      add 1 to j
    end

  add left[i to end] to result // question here, since 'add [5, 4, 3] to list' would make list [..., [5, 4, 3]], can we make 'add 5 to 3 to list'  make list [..., 5, 4, 3], do you get what I mean ?
  add right[j to end] to result // also as you saw, is it possible to make this work ? the way 'end' is used here, and to make the operation return the correct portion of the list, or none at all if the ranges are out of range ?

  give result
end

let x be 1
until x >= 3:
  show x, 5i

show "message" unless it's length is 3 then "hello, there"

let max take (a, b):
  give a unless b > a then b
  give a > b ? a : b
end

let length of (list) give list's length

(i to j) is j - i + 1 long
(1 to 3)'s length

let list be [1, 2, 3]
show length(list)

type Node is (data, next):
  data is data,
  next is next
end

let node be { 3, nil }
node[1 to 3]
node[5 to 1] => []

let node be Node(1, nil)

let numbers be ["one", "two", "three", "four"]

let i be 1
until i > numbers' length:
  show "`i`: `numbers.i`"
  add 1 to i
end

until node is nil:
  print node's data
  set node to node's next
end


show x * list[1]
show x * list.1

let list be [10, 20, 30, 40]

show list[1 to 3] => [10, 20, 30]
show list[2 to 1] => [20, 10]
show list[end to 3] => [nil, 40, 30]
print list[5] => nil // or should we make it error ? yup, we should make it error

let add (a, b) give a + b
let max of (a, b) give a if a > b else b
let max in (a, b, c) with (d) give a if a > b else b if b > c else c
show max of (2, 3) * 30 // should be able to tell that max is followed by 'of', and then (2, 3) and then nothing

a ( a , a ) * 3
TOKEN_IDENT TOKEN_LEFT_BRACE TOKEN_IDENT TOKEN_COMMA TOKEN_IDENT
TOKEN_RIGHT_BRACE TOKEN_STAR TOKEN_NUMBER

let max of (a, b) give a if a > b else b

TOKEN_LET TOKEN_IDENT TOKEN_IDENT, TOKEN_LEFT_BRACE TOKEN_IDENT TOKEN_COMMA TOKEN_IDENT TOKEN_IDENT
TOKEN_RIGHT_BRACE TOKEN_GIVE TOKEN_IDENT TOKEN_IF TOKEN_IDENT TOKEN_GREATER TOKEN_IDENT TOKEN_ELSE TOKEN_IDENT TOKEN_NEWLINE

let factorial of (a) give 1 if a <= 1 else a * factorial of (a - 1)

let max of (a, b) be a if a > b else b
show max of (22, 50)

let merge (a) and (b):
  let result be []
  let i, j be 1

  until i > a's length or j > b's length
    if a.i <= b.j:
      add a[i] to result
      add 1 to i
    else:
      add b.j to result
      add 1 to j
    end

  add a[i to end] to result
  add b[j to end] to result

  give result
end

let the merge sort of (list):
  if list's length < 2 give list

  let mid be list's length / 2
  let left  be sort list[1 to mid]
  let right be sort list[mid + 1 to end]

  give merge left and right
end

show the merge sort of [ 4, 6, 5, 1, 8, 2, -3, 10, 18, 15, 30, 70, 45, 23 ]

let quick sort of (list):
  if list's length < 2 return list

  let pivot be list[1]
  let rest be list[2 to end]

  let less, greater be []

  for each item in rest add item to less if item < pivot else greater

  let result be quick sort of less
  add (pivot, quick sort of greater) to result

  give result
end

show quick sort of [5, 4, 1, 10, 9, 3, 8]
list's length (7) < 2 false
pivot is list[1]= 5
rest is list[2 to end]=[4, 1, 10, 9, 3, 8]

less, greater = []
for each item in rest
  less = [4, 1, 3,]
  greater= [10, 9, 8,]

result is quick sort of less: [4, 1, 3,]
  list's length (3) < 2 false
  pivot is list[1]= 4
  rest is list[2 to end]=[1, 3]
  less, greater = []
  for each item in rest
    less = [1, 3]
    greater = []

  result is quick sort of less: [1, 3]
    list's length (2) < 2 false
    pivot is list[1] = 1
    rest is list[2 to end]=[3]
    less, greater = []
    for each item in rest
      less = []
      greater = [3]

    result is quick sort of less: []
      list's length (0) > 2 true
    add (pivot=1, quick sort of greater=[3]) to result
      quick sort of [3]
        list's length (1) < 2 true
    add (1, [3]) to result=[1, 3]

  add (pivot=4, quick sort of greater=[]) to result=[1, 3]
    quick sort of []
      list's length (0) < 2 true
  add (4, []) to result=[1, 3, 4]
  return result=[1, 3, 4]

add (pivot=5, quick sort of greater=[10, 9, 8]) to result=[1, 3, 4]
  quick sort of [10, 9, 8,]
    list's length (3) < 2 false
    pivot is list[1]=10
    rest is list[2 to end]=[9, 8]
    less, greater = []
    for each item in rest
      less = [9, 8]
      greater = []

    result is quick sort of less=[9, 8]
      quick sort of [9, 8]
        list's length (2) < 2 false
        pivot is list[1] = 9
        rest is list[2 to end] = [8]
        less, greater = []
        for each item in rest
          less = [8]
          greater = []

        result is quick sort of less=[8]
          quick sort of [8]
            list's length (1) < 2 true
        result is [8]
        add (pivot=9, quick sort of greater=[]) to result=[8]
          quick sort of []
            list's length (0) < 2 true
        add (9, []) to result = [8, 9]
        return result=[8, 9]
    result is [8, 9]
    add (pivot=10, quick sort of greater=[]) to result=[8, 9]
      quick sort of []
        list's length (0) < 2 true
    add (10, []) to result = [8, 9, 10]
    return result=[8, 9, 10]
add (5, [8, 9, 10]) to result=[1, 3, 4]
return result=[1, 3, 4, 5, 8, 9, 10]

