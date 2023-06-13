#!/bin/bash
#
# Copyright 2023 Anthony Paul Astolfi
#
# conan-config-flags.sh - print conan flags for install/create to stdout (Conan 1.x only)
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

script_dir=$(cd $(dirname $0) && pwd)
source "${script_dir}/common.sh"

conan_profile=$(test -f '/etc/conan_profile.default' && echo '/etc/conan_profile.default' || echo 'default')

echo -n "--profile \"${conan_profile}\" -s build_type=${BUILD_TYPE:-Release}"
