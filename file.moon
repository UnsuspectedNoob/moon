let cache (n):
  give "Cache: running `n`"
end

let build cache (n):
  give "Build Cache: running `n`"
end

let build (n):
  give "Build: running `n`"
end

show build 20
show cache 30
show build cache 50
show build (cache 300)
