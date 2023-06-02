# jq template that takes as input a single string, the build type (e.g., "Debug"),
# and produces a build task object to go in .vscode/tasks.json
#
{
  "version": "2.0.0",
  "tasks" : (.| map([
    {
      "label" : ("Build " + $ENV["package_name"] + " (" + . + ")"),
      "type" : "shell",
      "command" : ("make BUILD_TYPE=" +.+ " install build"),
      "problemMatcher" : ["$gcc"],
      "options" : {
        "cwd" : $ENV["project_dir"] 
      },
      "group" : {
        "kind" : "build",
         "isDefault" : (. == "RelWithDebInfo")
      }
    },
    {
      "label" : ("Clean " + $ENV["package_name"] + " (" + . + ")"),
      "type" : "shell",
      "command" : ("make BUILD_TYPE=" +.+ " clean"),
      "problemMatcher" : ["$gcc"],
      "options" : {
        "cwd" : $ENV["project_dir"] 
      },
      "group" : {
        "kind" : "build",
         "isDefault" : false
      }
    }
  ])) | flatten
}
