#!/bin/bash
#
set -e

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
    devel ${latest_release}
    exit 0
fi

echo $latest_release
