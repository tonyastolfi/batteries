#!/bin/bash
#
# Copyright 2022-2023 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

script_dir=$(cd $(dirname $0) && pwd)

source "${script_dir}/conan-version.sh"

output_format=${FORMAT:-json}
export ONLY=${ONLY:-'.*'}
export EXCLUDE=${EXCLUDE:-'^$'}

# The first arg is the project directory; if not specified, try to
# find the git top-level directory.
#
project_dir=${1:-$(git rev-parse --show-toplevel)}
cd "${project_dir}"
project_dir=$(pwd)

test -d "$project_dir" || {
    echo "Warning: Skipping non-directory: '${project_dir}'"
    exit 0
}

if [ "${conan_version_2}" == "1" ]; then

    if [ "${VERBOSE:-}" == "1" ]; then
        stderr_redirect=
    else
        stderr_redirect=2\>/dev/null
    fi
    bash -c "conan graph info -f json . $stderr_redirect" \
          | jq '.graph|.nodes|.["0"]|.dependencies|to_entries|map(.value|select(.direct=="True")|.ref)'
else

    project_name=$(conan inspect --raw=name "${project_dir}")
    project_version=$(conan inspect --raw=version "${project_dir}")

    deps_json=$(conan inspect --raw=requires "$project_dir" \
                    | sed -e "s,',\",g" \
                    | jq "{\"${project_name}/${project_version}\": .}")


    if [ "${NAME_ONLY:-0}" == "1" ]; then
        deps_json=$(echo "$deps_json" \
                        | jq 'to_entries|map({key: (.key | sub("/.*$"; "")), value: (.value | map(sub("/.*$"; "")))})|from_entries')
    fi

    if [ "$output_format" == "json" ]; then
        echo "$deps_json"
        #+++++++++++-+-+--+----- --- -- -  -  -   -
    fi
fi



