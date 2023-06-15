#!/bin/bash
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# Configuration Params

#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# Include the common script.
#
script_dir=$(cd $(dirname $0) && pwd)
source "${script_dir}/common.sh"

# Check for jq.
#
which jq >/dev/null || {
    echo "Skipping vscode config generation (requires jq; not found)" >&2
    exit 0
}

# build_types - a list of the default build configuration types supported by
# cmake/conan.
#
build_types=($(jq -r -M ".[]" "${script_dir}/lib/build_types.json"))

#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# Use the project directory name as the package name.
#
export package_name=$(basename "${project_dir}")
export project_dir=${project_dir}

#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# Calculate the .vscode config files directory.
#
vscode_config_dir=${project_dir}/.vscode

echo "vscode_config_dir is ${vscode_config_dir}" >&2

mkdir -p "${vscode_config_dir}"

#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# Build .vscode/tasks.json (first time only)
#
if [ ! -f "${vscode_config_dir}/tasks.json" ]; then
    cat "${script_dir}/lib/build_types.json" \
        | jq -M -f "${script_dir}/lib/vscode.tasks.json.jq" \
             > "${vscode_config_dir}/tasks.json"
fi

#----- --- -- -  -  -   -

os_name=$(uname -s)
if [ "${os_name}" == "Darwin" ]; then
    with_execute_permission="-perm -0111"
else
    with_execute_permission="-perm /111"
fi

#+++++++++++-+-+--+----- --- -- -  -  -   -
# build_info_json
#
# json text - an array of objects of the form:
#
# [
#   {
#     "build_type": "Debug",
#     "commands": [ ... ]
#   },
#   ...
# ]
#
# The "commands" property value is the contents of a compile_commands.json file.
# build_info_json is populated for all build types for which we are able to find
# a valid build/BUILD_TYPE/compile_commands.json file.
#
echo "project_dir is ${project_dir}"
for build_type in ${build_types[@]}; do
    export build_type=${build_type}
    
    compile_commands_file=${project_dir}/build/${build_type}/compile_commands.json
    user_test_env_file=${project_dir}/build/user_test_env.${build_type}.json
    
    echo "checking for ${compile_commands_file}"

    if [ -e "${compile_commands_file}" ]; then    
        find "${project_dir}/build/${build_type}" -type f ${with_execute_permission} \
            | grep -iE 'test' \
            | xargs -I {} bash -c "export file={} ; echo \$file ; echo \$(basename \$file)" \
            | jq -R \
            | jq --slurp -f "${script_dir}/lib/test_files.jq" \
                 > "${project_dir}/build/vscode.${build_type}.tests.json"

        if [ ! -f "${user_test_env_file}" ]; then
            jq -n -M -f "${script_dir}/lib/default_test_env.jq" \
               > "${user_test_env_file}"
        fi

        {
            jq -n -M -f "${script_dir}/lib/default_test_env.jq" \
               | jq -M '. | to_entries | .[]'
            jq -M '. | to_entries | .[]' "${user_test_env_file}"
        } | jq -M --slurp 'from_entries' \
               > "${project_dir}/build/vscode.${build_type}.test_env.json"

        cat "${project_dir}/build/vscode.${build_type}.tests.json" \
            "${project_dir}/build/vscode.${build_type}.test_env.json" \
            | jq -M --slurp '.[0] as $tests | .[1] as $test_env | $tests | map(. + { env: $test_env })' \
                 > "${project_dir}/build/vscode.${build_type}.launch.json"

        cat "${compile_commands_file}" \
            | jq -M -f "${script_dir}/lib/compile_info.jq" \
                 > "${project_dir}/build/vscode.${build_type}.compile_info.json"
    fi
done

#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# Build .vscode/launch.json
#
find build -name 'vscode.*.launch.json' \
    | xargs cat \
    | jq --slurp 'flatten' \
    | jq -M -f "${script_dir}/lib/vscode.launch.json.jq" \
         > "${vscode_config_dir}/launch.json"

#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# Build .vscode/c_cpp_properties.json
#
find build -name 'vscode.*.compile_info.json' \
    | xargs cat \
    | jq -M --slurp '.' \
    | jq -M -f "${script_dir}/lib/vscode.c_cpp_properties.json.jq" \
         > "${vscode_config_dir}/c_cpp_properties.json"
