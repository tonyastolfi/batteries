#!/bin/bash -x
#
# Copyright 2022 Anthony Paul Astolfi
#
set -e

script_dir=$(cd $(dirname $0) && pwd)
source "${script_dir}/common.sh"

# Create a local conan cache dir so we can use GitLab file cache.
#
if [ ! -e "${local_conan_dir}" ]; then
    cp -r "${default_conan_dir}" "${local_conan_parent_dir}"
fi

cd "${project_dir}"

CONAN_USER_HOME="${local_conan_parent_dir}" make BUILD_TYPE=Release "$@"
