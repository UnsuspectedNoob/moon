
show "=========================================="
show "       THE GRAND ITERATION TEST           "
show "=========================================="

show "1. Dictionary Unpacking (The New Magic!)"
let user be { name: "Emrys", role: "Admin", level: 99 }

for each key, val in user show "  [+] `key` : `val`"


show "
2. String Unpacking (1-Based Indexing Check)"
let lang be "MOON"

for each char, pos in lang:
    show "  Letter `pos` is `char`"
end


show "
3. List Unpacking"
let weapons be ["Sword", "Bow", "Staff"]

for each weapon, idx in weapons:
  show "  Slot `idx`: `weapon`"
end

show "=========================================="
show "ALL ITERATORS FUNCTIONAL."
