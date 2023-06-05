# test_files.jq - The set of unit test binaries for a single BUILD_TYPE
#
# input: array of strings, odd elements are test binary paths, even are names
# env:
#  - build_type: one of "Debug", "Release", "RelWithDebInfo"
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
