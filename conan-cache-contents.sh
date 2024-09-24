#!/bin/bash
#
# Lists the packages (full refs) in the local Conan cache.
#
set -Eeuo pipefail

set -x

conan list --cache --format compact '*:*' \
    | grep -E '/.*#[0-9a-f]+:[0-9a-f]+' \
    | grep -v 'requires:' \
    | sed -E 's,$ +,,g' \
    | sort \
    || true
