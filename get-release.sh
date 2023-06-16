#!/bin/bash
#
# Copyright 2022 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

script_dir=$(cd "$(dirname "$0")" && pwd)
source "${script_dir}/common.sh"

version_from_release_tag $(find_release_tag)
