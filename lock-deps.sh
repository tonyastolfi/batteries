#!/bin/bash
#
# lock-deps.sh
#
# Creates or updates the conan.lock lockfile in the project root dir.
#
# Requires the file supported_platforms.json at the top level project
# dir.  This file should contain a JSON array of objects, each of which
# contains a list of key/value pairs used to generate a '-s' argument
# to pass to conan lock create.  For example:
#
# [
#   {
#     "os": "Linux",
#     "arch": "x86_64",
#     "compiler": "gcc",
#     "compiler.version": "11"
#   },
#   {
#     "os": "Macos",
#     "compiler": "apple-clang",
#     "compiler.version": "14"
#   }
# ]
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

script_dir="$(cd "$(dirname "$0")" && pwd)"

source "${script_dir}/common.sh"
if [ -f "${project_dir}/supported_platforms.json" ]; then
    cat supported_platforms.json \
        | jq -r '.[]|to_entries|map("-s " + .key + "=" + .value)|join(" ")' \
        | xargs -t -L 1 "${script_dir}/conan-lock-merge.sh"

    cat "${project_dir}/conan.lock" \
        | jq '.requires|=sort | .build_requires|=sort | .python_requires|=sort | .config_requires |=sort' \
             > "${project_dir}/tmp.conan.lock"

    mv -f "${project_dir}/tmp.conan.lock" "${project_dir}/conan.lock"
else
    echo "Error: project missing file 'supported_platforms.json'" >2
    exit 1
fi
