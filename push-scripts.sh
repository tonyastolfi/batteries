#!/bin/bash
#
# Copyright 2023-2024 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

script_dir=$(cd "$(dirname $0)" && pwd)
extra_args=

if [ "${FORCE:-}" == "1" ]; then
    extra_args=-f
fi

cd "${script_dir}"
git remote | xargs -n 1 -I {} git push ${extra_args} {} HEAD:script
