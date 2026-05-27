
type Book:
  name,
  author,
  read: 0
end

let book be Book {
  name: "1984",
  author: "George Orwell"
}

show book's author
update book's read + 1
show book's read

