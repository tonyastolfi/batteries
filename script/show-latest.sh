#!/bin/bash
#
# Copyright 2022-2023 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

{
    package_name=$1
    output=$(conan search "${package_name}" -r all --raw \
                 | grep "${package_name}")
} || {
    echo "Error: no packages found for name '${package_name}' (stale conan login?)" >&2
    exit 1
}

echo "${output}" \
    | grep -v "JulesTheGreat" \
    | grep -v "gtest/cci" \
    | sort -h \
    | sort -Vr \
    | head -1
