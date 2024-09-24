#!/bin/bash
#
# Lists the packages (full refs) in the local Conan cache.
#
set -Eeuo pipefail

conan list --cache --format compact '*:*' 2>/dev/null \
    | grep -E '/.*#[0-9a-f]+:[0-9a-f]+' \
    | grep -v 'requires:' \
    | sed -E 's,$ +,,g' \
    | sort
