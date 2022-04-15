#!/bin/bash
#
# Copyright 2022 Anthony Paul Astolfi
#
# Usage: script/tag-release.sh <"major"|"minor"|"patch"(default)>
#
# Tags the current commit for release.  The working tree must be
# clean!  This doesn't actually perform the release (i.e., conan
# create/upload), it just creates a git tag that will trigger a
# release when CI/CD pipelines run.
#
set -e

script_dir=$(cd $(dirname $0) && pwd)
source "${script_dir}/common.sh"

script=$0
release_type=${1:-"patch"}
current_version=$(VERBOSE=0 "${script_dir}/get-version.sh")
next_version=$(find_next_version "${current_version}" "${release_type}")

echo "${current_version} => ${next_version}"

command="git tag -m '(no message)' -a release-${next_version} HEAD"

if [ "$DRY_RUN" = "1" ]; then
    echo $command
    working_tree_is_clean || {
        echo $(cat <<EOF
               Warning: command will fail because the working tree is dirty.
EOF
            ) >&2
    }
else
    working_tree_is_clean || {
        echo $(cat <<EOF
               Error: the working tree has uncommitted changes; please commit
               all and retry.
EOF
               ) >&2
        exit 1;
    }

    # All checks have passed!  Apply the tag.
    #
    bash -c "$command"
fi
