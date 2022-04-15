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

# If the working tree is dirty, then show the next patch version with "-devel" appended.
#
working_tree_is_clean || {
    verbose "The working tree is dirty; adding '+1-devel' to latest release '${latest_release}'"
    devel "${latest_release}"
    exit 0
}

# Get the latest commit hash and the commit hash of the latest release tag.
#
latest_commit=$(git rev-list -n 1 HEAD | awk '{print $1}')
latest_release_commit=$(git rev-list -n 1 "${latest_release_tag}" | awk '{print $1}')

# If the working tree is clean but our branch is ahead of the release tag, then we also
# want to emit the devel version.
#
if [ "${latest_commit}" != "${latest_release_commit}" ]; then
    verbose "HEAD is ahead of last release tag '${latest_release_tag}'; adding '+1-devel' to latest release '${latest_release}'"
    devel "${latest_release}"
    exit 0
fi

# This truly is the current release!
#
echo $latest_release
