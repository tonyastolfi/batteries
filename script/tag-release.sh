#!/bin/bash
#
# Copyright 2022 The MathWorks, Inc.
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

tag_command="git tag -m '(no message)' -a release-${next_version} HEAD"
conan_version=$(find_conan_version)
target_conan_version=$(OVERRIDE_RELEASE_TAG="release-${next_version}" find_conan_version)

push_command="git push origin release-${next_version}"
if [ "$A8_PUSH_MAIN_WITH_RELEASE" == "1" ]; then
    push_command="${push_command} main"
fi

if [ "$DRY_RUN" == "1" ]; then
    if [ "$A8_COPY_COMMANDS_TO_CLIPBOARD" == "1" ] && [ "$(uname)" == "Darwin" ]; then
        echo "# ${push_command}" | pbcopy
    fi

    echo $tag_command
    will_fail=0
    working_tree_is_clean || {
        will_fail=1
        echo $(cat <<EOF
               Warning: command will fail because the working tree is dirty.
EOF
            ) >&2
    }
    [ "${target_conan_version}" == "${next_version}" ] || {
        will_fail=1
        echo $(cat <<EOF
               Warning: command will fail because the conan version
               (${conan_version}) does not match the target release version
               (${next_version}).
EOF
            )
    }
    if [ "$will_fail" == "1" ]; then
        echo "(No action taken; fix all warnings and re-run without DRY_RUN=1 to apply changes)"
    else
        echo "(No action taken; re-run without DRY_RUN=1 to apply changes)"
    fi
else
    working_tree_is_clean || {
        echo $(cat <<EOF
               Error: the working tree has uncommitted changes; please commit
               all and retry.
EOF
               ) >&2
        exit 1;
    }

    [ "${target_conan_version}" == "${next_version}" ] || {
        echo $(cat <<EOF
               Error: the conan version (${conan_version}) does not match the
               target release version (${next_version}); please fix and try
               again!
EOF
            )
        exit 1;
    }

    # All checks have passed!  Apply the tag.
    #
    bash -c "${tag_command}"

    echo "Release has been tagged; to publish this version, run:"
    echo
    echo "${push_command}"
    echo

    # Copy the push command to the clipboard if requested.
    #
    if [ "$A8_COPY_COMMANDS_TO_CLIPBOARD" == "1" ] && [ "$(uname)" == "Darwin" ]; then
        echo "${push_command}" | pbcopy
        echo "(command copied to clipboard)"
    fi
fi
