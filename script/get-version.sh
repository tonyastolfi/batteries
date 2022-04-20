#!/bin/bash
#
# Copyright 2022 Anthony Paul Astolfi
#
set -e

script_dir=$(cd $(dirname $0) && pwd)
source "${script_dir}/common.sh"

# Search for the latest release tag reachable from the current branch.
#
latest_release_tag=$(find_release_tag)
latest_release=$(version_from_release_tag "${latest_release_tag}")

function devel {
    echo $(find_next_version "$1" patch)-devel
}

if [ "${latest_release}" == "0.0.0" ]; then
    verbose "No git release tag found; using 0.1.0-devel as the initial version"
    active_version="0.1.0-devel"
elif  ! working_tree_is_clean ; then
    # If the working tree is dirty, then show the next patch version with
    # "-devel" appended.
    #
    verbose $(cat <<EOF
              The working tree is dirty; adding '+1-devel' to latest release
              '${latest_release}'
EOF
        )
    active_version=$(devel "${latest_release}")
else
    # Get the latest commit hash and the commit hash of the latest release
    # tag.
    #
    latest_commit=$(git rev-list -n 1 HEAD | awk '{print $1}')
    latest_release_commit=$(git rev-list -n 1 "${latest_release_tag}" \
                                | awk '{print $1}')

    # If the working tree is clean but our branch is ahead of the release
    # tag, then we also want to emit the devel version.
    #
    if [ "${latest_commit}" != "${latest_release_commit}" ]; then
        verbose $(cat <<EOF
                  HEAD is ahead of last release tag '${latest_release_tag}';
                 adding '+1-devel' to latest release '${latest_release}'
EOF
            )
        active_version=$(devel "${latest_release}")
    else
        active_version=$latest_release
    fi
fi

# If the conan version doesn't match, error!
#
if [ "${NO_CHECK_CONAN}" != "1" ]; then
    conan_version=$(find_conan_version)
    if [ "${conan_version}" != "${active_version}" ]; then
        echo $(cat <<EOF
               Error: conan version (${conan_version}) does not match inferred
               version from git (${active_version})!  Please resolve this issue
               and try again.
EOF
        ) >&2
        exit 1
    fi
fi

# Success!
#
echo $active_version
