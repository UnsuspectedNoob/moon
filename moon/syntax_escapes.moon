let msg be "Hello\nWorld\t!\nQuote: \"Moon\"\nSlash: \\"
show msg

let count be 10
set count to 0 unless it > 5
show count

let list be [1, 2, 3, 4, 5]
let result be [for each x in list keep x if it >= 3]
show result
show "Result length: `result's length`"
