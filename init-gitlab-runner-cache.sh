#!/bin/bash
#
# Copyright 2023 Anthony Paul Astolfi
#
if [ "$(uname -o)" != "Darwin" ]; then
    function copy_if_newer() {
        if [ "$1" -nt "$2" ]; then
            cp "$1" "$2"
        fi
    }

    local_cache_dir=/local/gitlab-runner-local-cache
    (
        set -Eeuo pipefail

        test -d "${local_cache_dir}" || {
            echo "FATAL: no local docker cache volume mapped"
            exit 1
        }

        conan_version_2=$({ conan --version | grep -i 'conan version 2' >/dev/null ; } && echo 1 || echo 0)

        if [ "${conan_version_2}" == "1" ]; then
            conan_dst_dir=${local_cache_dir}/.conan2
            conan_src_dir=${CONAN_HOME:-${HOME}/.conan2}
        else
            conan_dst_dir=${local_cache_dir}/.conan
            conan_src_dir=${CONAN_USER_HOME:-${HOME}}/.conan
        fi

        conan_tmp_dir=${conan_dst_dir}.IN_PROGRESS

        if [ ! -d "${conan_dst_dir}" ]; then
            rm -rf "${conan_tmp_dir}"
            cp -r "${conan_src_dir}" "${conan_tmp_dir}"
            mv "${conan_tmp_dir}" "${conan_dst_dir}"
        fi

        copy_if_newer "${conan_src_dir}/settings.yml" "${conan_dst_dir}/settings.yml"
    )
    export CONAN_USER_HOME=${local_cache_dir}

    # Add batteriesincluded remote if not present.
    {
        conan remote list | grep batteriesincluded
    } || {
        conan remote add batteriesincluded https://batteriesincluded.cc/artifactory/api/conan/conan-local
        conan remote list | grep batteriesincluded
    } || {
        echo "Failed to add conan remote 'batteriesincluded'" >&2
        exit 1
    }
fi
