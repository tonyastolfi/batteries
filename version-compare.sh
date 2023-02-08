#!/bin/bash
#
# Copyright 2022-2023 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

left_value=${1}
right_value=${2}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Simplest case; the two inputs are equal.
#
if [ "${left_value}" == "${right_value}" ]; then
    exit 0
fi

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Prefer (i.e. consider to be "newer") refs without '@user/channel'.
#
function just_package_name_and_version() {
    echo $1 | sed -E 's,@.*$,,g'
}

if [ "$(just_package_name_and_version ${left_value})" == "${right_value}" ]; then
    exit 0
fi

if [ "$(just_package_name_and_version ${right_value})" == "${left_value}" ]; then
    exit 1
fi

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# These are a little counter-intuitive to read...
#
# if left is X-devel and right is X...
#
if [ "${left_value}" == "${right_value}-devel" ]; then
    exit 0
fi

# if left is X and right is X-devel...
#
if [ "${left_value}-devel" == "${right_value}" ]; then
    exit 1
fi

function echo_versions() {
    echo $1
    echo $2
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
if [ "$(echo_versions $left_value $right_value)" \
         == "$(echo_versions $right_value $left_value) | sort -V)" ]; then
    exit 0
else
    exit 1
fi
