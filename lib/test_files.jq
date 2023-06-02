# input: array of strings, odd elements are test binary paths, even are names
# output: array of objects like { path: ..., name: ... }
#
[
  . as $all |
  range(0; ($all | length); 2) |
  {
    path: ($all[.]),
    name: ($all[.+1]),
    build_type: $ENV["build_type"]                       
  }
]
