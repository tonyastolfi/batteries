#!/bin/bash
#
set -e

function verbose {
    if [[ "$VERBOSE" == "1" ]]; then
        echo "($@)" >&2
    fi
}

# Search for the latest release tag reachable from the current branch.
#
latest_release_tag=$(git tag --list --merged HEAD --sort=-version:refname release-* | head -1)
latest_release=$(echo "$latest_release_tag" | sed -e 's,release-,,g')

function devel {
    prev_version=$1
    major_minor=$(echo "$1" | sed -e 's,\., ,g' | awk '{ print $1 "." $2; }')
    patch_num=$(echo "$1" | sed -e 's,\., ,g' | awk '{ print $3; }')
    next_patch=$(expr $patch_num \+ 1)
    echo "${major_minor}.${next_patch}-devel"
}

# If the working tree is dirty, then show the next patch version with "-devel" appended.
#
if [[ -n $(git status --short) ]]; then
    verbose "The working tree is dirty; adding '+1-devel' to latest release '${latest_release}'"
    devel ${latest_release}
    exit 0
fi

# Get the latest commit hash and the commit hash of the latest release tag.
#
latest_commit=$(git rev-list -n 1 HEAD | awk '{print $1}')
latest_release_commit=$(git rev-list -n 1 "${latest_release_tag}" | awk '{print $1}')

# If the working tree is clean but our branch is ahead of the release tag, then we also
# want to emit the devel version.
#
if [ "${latest_commit}" != "${latest_release_commit}" ]; then
    verbose "HEAD is ahead of last release tag '${latest_release_tag}'; adding '+1-devel' to latest release '${latest_release}'"
    devel ${latest_release}
    exit 0
fi

# This truly is the current release!
#
echo $latest_release
