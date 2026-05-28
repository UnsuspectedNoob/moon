let fruits be ["apple", "banana", "cherry", "date"]

# Using end to slice to the end
show fruits[1 to end]

# Using end to slice from the end
show fruits[end to 0]

# Using end as a direct index
show fruits[end]

# Using end math
show fruits[end - 1]

# expect: [apple, banana, cherry, date]
# expect: [date, cherry, banana, apple]
# expect: date
# expect: cherry

let word be "Moonlight"
show word[1 to end]
show word[5 to end]
show word[end to 1]

# expect: Moonlight
# expect: light
# expect: thgilnooM
