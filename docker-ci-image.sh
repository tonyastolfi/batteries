#!/bin/bash
#
# Copyright (C) 2023 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

# Use this script to figure out where the project is.
#
script_dir=$(cd "$(dirname $0)" && pwd)
source "${script_dir}/common.sh"

# Pull the docker image string out of the gitlab CI definition.
#
cat "${project_dir}/.gitlab-ci.yml" | grep -E '^image:' | sed -e 's,image:\( *\),,g'
