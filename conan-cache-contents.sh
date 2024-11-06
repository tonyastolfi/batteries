#!/bin/bash
#
# Lists the packages (full refs) in the local Conan cache.
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

conan list --cache --format compact '*:*' \
    | { grep -E '/.*#[0-9a-f]+:[0-9a-f]+' || true; } \
    | { grep -v 'requires:' || true; } \
    | sed -E 's,$ +,,g' \
    | sort
