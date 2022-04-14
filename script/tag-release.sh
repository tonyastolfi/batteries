#!/bin/bash
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
release_type=${1:-patch}
current_version=$(VERBOSE= "${script_dir}/get-version.sh")
next_version=$(find_next_version "${current_version}" "${release_type}")

echo "${current_version} => ${next_version}"

command=$(echo git tag -m \'"$USER ran ${script}"\' -a release-${next_version} HEAD)

if [ "$DRY_RUN" == "1" ]; then
    echo $command
else
    working_tree_is_clean || {
        echo "Error: the working tree has uncommitted changes; please commit all and retry." >&2
        exit 1
    }
    $command
fi
