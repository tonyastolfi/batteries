#------------------------------------------------------------------------------
# include_path_args - translates a two-element array with adjacent compiler
#     flags into an include path (or nothing)
#
def include_path_args:
  if (.[0] | startswith("-I")) then
    ["-isystem", (.[0]|.[2:])]
  else
    .
  end |
  if (.[0] == "-isystem") then
    .[1]
  else
    null
  end |
  select(strings);

#------------------------------------------------------------------------------
# The c_cpp_properties.json object.
#
{
    "configurations": map(. | {
            "name": (.build_type + "(gcc)"),
            "includePath": (
              .commands[0].command |
              split(" ") |
              [.[0:], .[1:]] |
              transpose |
              map(include_path_args)
            ),
            "defines": (.commands[0].command | split(" ") | map(select(startswith("-D")) | .[2:])),
            "compilerPath": (.commands | .[0].command | split(" ") | .[0]),
            "compilerArgs": [],
            "cStandard": "c17",
            "cppStandard": "c++20",
            "intelliSenseMode": "${default}",
            "compileCommands": (.project_dir + "/build/" + .build_type + "/compile_commands.json")
        }),
    "version": 4
}
