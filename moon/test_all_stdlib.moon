show "--- Testing Core Library ---"
let time be clock
show "Current clock:"
show time

show "--- Testing Math Library ---"
show "sin(0):" 
show sin 0
show "cos(0):" 
show cos 0
show "sqrt(16):" 
show square root of 16
show "2^3:" 
show power of 2 to 3
show "floor(3.9):" 
show floor of 3.9
show "random(1,10):" 
show random from 1 to 10

show "--- Testing String Library ---"
show "uppercase:" 
show uppercase "hello"
show "lowercase:" 
show lowercase "WORLD"
show "trim:" 
show trim "   spaces   "
show "split:" 
show split "a,b,c" by ","

show "--- Testing List Library ---"
let my_list be [1, 2, 3]
show "original:" 
show my_list
show "reversed:" 
show reverse my_list
show "join:" 
show join my_list with " - "
show "pop:" 
show pop from my_list
show "after pop:" 
show my_list
show "index of 1:" 
show index of 1 in my_list
show "binary 101:" 
show numbers in "101" in base 2

show "--- Testing IO Library ---"
write "Hello" to "test_io.txt"
append " World" to file "test_io.txt"
if file "test_io.txt" exists:
    show "File contents:" 
    show read file "test_io.txt"
end

show "--- All Tests Completed Successfully ---"
