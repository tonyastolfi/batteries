#!/bin/bash
#
# Copyright 2023 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

git remote | xargs -n 1 -I {} git push {} script
