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

tools_dir="$(cd "$(dirname "$0")" && realpath .)"
source "${tools_dir}/common.sh"

build_dir="${project_dir}/build"
supported_platforms_file="${project_dir}/supported_platforms.json"
lock_file="${project_dir}/conan.lock"
tmp_lock_file="${project_dir}/tmp.conan.lock"

# Clean up any old left-over tmp lockfiles.
#
rm -f "${tmp_lock_file}"

# If CLEAN=1 env var is set, then remove existing lockfile first.
#
if [ "${CLEAN:-0}" == "1" ]; then
    rm -f "${lock_file}"
fi

conan lock create --build=missing "--lockfile-out=${lock_file}" "${project_dir}"

cat "${lock_file}" \
    | jq 'to_entries|map(.value |= (if type == "array" then map(if type == "string" then sub("#.+"; "") else . end) else . end))|from_entries' \
         >"${tmp_lock_file}"

rm -f "${lock_file}"
mv -f "${tmp_lock_file}" "${lock_file}"

exit 0
#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# DEPRECATED
#
if [ -f "${supported_platforms_file}" ]; then

    # Enumerate the contents of 'supported_platforms.json'
    #
    cat "${supported_platforms_file}" \
        | jq -r '.[] | to_entries | map("-s " + .key + "=" + .value) | join(" ")' \
        | xargs -L 1 "${tools_dir}/conan-lock-merge.sh"

    filter=$(echo "$(cat <<EOF
      .requires        |= if type == "array" then sort else [] end |
      .build_requires  |= if type == "array" then sort else [] end |
      .python_requires |= if type == "array" then sort else [] end |
      .config_requires |= if type == "array" then sort else [] end
EOF
    )")

    cat "${lock_file}" | jq "${filter}" > "${tmp_lock_file}"
    mv -f "${tmp_lock_file}" "${lock_file}"
else
    echo "Error: project missing file '${supported_platforms_file}'" >&2
    exit 1
fi
