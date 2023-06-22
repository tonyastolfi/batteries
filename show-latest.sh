#!/bin/bash
#
# Copyright 2022-2023 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

script_dir=$(cd $(dirname $0) && pwd)
source "${script_dir}/common.sh"

{
    package_name=$1

    if [ "${conan_version_2}" == "1" ]; then
        output=$(conan search "${package_name}" -f json \
                     | jq -r 'to_entries | map(.value | to_entries | map(.key)) | flatten | .[]')
    else
        output=$(conan search "${package_name}" -r all --raw \
                     | grep "${package_name}")
    fi

} || {
    echo "Error: no packages found for name '${package_name}' (stale conan login?)" >&2
    exit 1
}

echo "${output}" \
    | grep -v "JulesTheGreat" \
    | grep -v "gtest/cci" \
    | grep -v -E "yaml-cpp.*\@signal9" \
    | sort -h \
    | sort -Vr \
    | head -1
