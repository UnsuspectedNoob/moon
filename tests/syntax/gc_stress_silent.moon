let i be 1

while i <= 5000:
  # Rapidly allocate dictionaries and concatenated strings
  # They immediately fall out of scope and become unreachable
  let junk be {
    "iteration": i,
    "text": "garbage " + "collection"
  }

  # Allocate an ephemeral array
  let temporary_list be [ 1, 2, 3, 4, 5 ]

  add 1 to i
end

# If we made it here without a segfault, the GC is perfectly stable!
