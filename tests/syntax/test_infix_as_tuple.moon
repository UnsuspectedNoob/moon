##
  test_infix_as_tuple.moon
  Tests for 'as' operator phrasal overload with leading tuple parameters and show method dispatch
##

type Date:
  day,
  month,
  year is 2026
end

let months be {
  "September": [ 9, 30 ]
}

let (m: String, d: Number) as day:
  give {
    day: d,
    month: months[m].1
  } as Date
end

let show (d: Date):
  show "Date: `d's month`/`d's day`/`d's year`"
end

let my date be ("September", 9) as day
show my date
# expect: Date: 9/9/2026

show ("September", 9) as day
# expect: Date: 9/9/2026
