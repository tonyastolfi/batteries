#!/bin/bash -x
#
set -e

script_dir=$(cd $(dirname $0) && pwd)
project_dir=$(cd ${script_dir}/.. && pwd)
local_conan_dir=${project_dir}/
default_conan_dir=${HOME}/.conan

# Create a local conan cache dir so we can use GitLab file cache.
#
if [ ! -e "${local_conan_dir}" ]; then
    cp -r "${default_conan_dir}" "${local_conan_dir}"
fi

cd "${project_dir}"

CONAN_USER_HOME="${local_conan_dir}" make BUILD_TYPE=Release "$@"
