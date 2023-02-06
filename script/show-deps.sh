#!/bin/bash
#
# Copyright 2022-2023 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

script_dir=$(cd $(dirname $0) && pwd)

# The first arg is the project directory; if not specified, try to
# find the git top-level directory.
#
project_dir=${1:-$(git rev-parse --show-toplevel)}
cd "${project_dir}"
project_dir=$(pwd)

test -d "$project_dir" || {
    echo "Warning: kipping non-directory: '${project_dir}'"
    exit 0
}

project_name=$(conan inspect --raw=name "${project_dir}")
project_version=$(conan inspect --raw=version "${project_dir}")

deps_json=$(conan inspect --raw=requires "$project_dir" | sed -e "s,',\",g" | jq "{\"${project_name}/${project_version}\": .}")
output_format=${FORMAT:-json}

export ONLY=${ONLY:-'.*'}
export EXCLUDE=${EXCLUDE:-'^$'}

if [ "${NAME_ONLY:-0}" == "1" ]; then
    deps_json=$(echo "$deps_json" | jq 'to_entries|map({key: (.key | sub("/.*$"; "")), value: (.value | map(sub("/.*$"; "")))})|from_entries')
fi

if [ "$output_format" == "json" ]; then
    echo "$deps_json"
    #+++++++++++-+-+--+----- --- -- -  -  -   -

fi
