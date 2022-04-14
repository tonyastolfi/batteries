#!/bin/bash
#
set -e

script_dir=$(cd $(dirname $0) && pwd)
source "${script_dir}/common.sh"

working_tree_is_clean || {
    echo "Error: the working tree has uncommitted changes; please commit all and retry." >&2
    exit 1;
}

active_version=$("${script_dir}/get-version.sh")
release_version=$("${script_dir}/get-release.sh")

if [ "$active_version" != "$release_version" ]; then
    echo "Error: active version (${active_version}) does not match the release version (${release_version}); did you forget to run ${script_dir}/tag-release.sh?" >&2
    exit 1
fi

if [ "$BATT_CONAN_UPLOAD_USER" == "" ]; then
    echo "Error: env var `BATT_CONAN_UPLOAD_USER` not specified!" >&2
    exit 1
fi
