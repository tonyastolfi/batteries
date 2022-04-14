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

function usage {
    echo "usage: $0 [major|minor|patch](default=patch)" >&2
    exit 1
}

if [[ -n $(git status --short) ]]; then
    echo "Error: the working tree has uncommitted changes; please commit all and retry." >&2
    exit 1
fi

script_dir=$(cd $(dirname $0) && pwd)
release_type=${1:-patch}
current_version=$("${script_dir}/get-version.sh")

function parse_version {
    version=$1
    part=$2
    echo "${version}" | sed -e 's,[.-], ,g' | awk "{ print \$${part}; }"
}

function find_next_version {
    current_version=$1
    release_type=$2
    major_num=$(parse_version "${current_version}" 1)
    minor_num=$(parse_version "${current_version}" 2)
    patch_num=$(parse_version "${current_version}" 3)
    devel_tag=$(parse_version "${current_version}" 4)
    
    if [ "${release_type}" == "major" ]; then
        echo "$(expr ${major_num} \+ 1).0.0"
    elif [ "${release_type}" == "minor" ]; then
        echo "${major_num}.$(expr ${minor_num} \+ 1).0"
    elif [ "${release_type}" == "patch" ]; then
        if [ "${devel_tag}" == "devel" ]; then
            echo "${major_num}.${minor_num}.${patch_num}"
        else
            echo "${major_num}.${minor_num}.$(expr ${patch_num} \+ 1)"
        fi
    else
        usage
    fi
}

next_version=$(find_next_version "${current_version}" "${release_type}")

echo "${current_version} => ${next_version}"

git tag -a release-${next_version}
