#!/bin/bash
#
# Copyright 2023 Anthony Paul Astolfi
#
if [ "$(uname -o)" != "Darwin" ]; then 
    local_cache_dir=/local/gitlab-runner-local-cache
    (
        set -Eeuo pipefail
        
        test -d "${local_cache_dir}" || {
            echo "FATAL: no local docker cache volume mapped"
            exit 1
        }
        
        conan_dir=${local_cache_dir}/.conan 
        tmp_conan_dir=${conan_dir}.IN_PROGRESS
        
        if [ ! -d "${conan_dir}" ]; then
            rm -rf "${tmp_conan_dir}"
            cp -r ~/.conan "${tmp_conan_dir}"
            mv "${tmp_conan_dir}" "${conan_dir}"
        fi
        
        if [ ~/.conan/settings.yml -nt "${conan_dir}/settings.yml" ]; then
            cp ~/.conan/settings.yml  "${conan_dir}/settings.yml"
        fi
        
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
