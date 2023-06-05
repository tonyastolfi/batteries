# compile_info.jq - Compilation settings for a single BUILD_TYPE
#
# input: contents of a compile_commands.json
# env:
#  - build_type: one of "Debug", "Release", or "RelWithDebInfo"
#  - project_dir: the top-level git repo directory of the project
#
{
  build_type: $ENV["build_type"],
  project_dir: $ENV["project_dir"],                              
  commands: .[:1]
}
