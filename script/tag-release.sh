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
current_version=$(VERBOSE=0 NO_CHECK_CONAN=1 "${script_dir}/get-version.sh")
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

    conan_version=$(OVERRIDE_RELEASE_TAG="release-${next_version}" find_conan_version)
    [ "${conan_version}" == "${next_version}" ] || {
        echo $(cat <<EOF
               Error: the conan version (${conan_version}) does not match the
               target release version (${next_version}); please fix and try
               again!
EOF
            )
    }

    # All checks have passed!  Apply the tag.
    #
    bash -c "$command"

    echo "Release has been tagged; to publish this version, run:"
    echo
    echo "git push origin release-${next_version}"
    echo
fi
