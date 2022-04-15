######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
# Copyright 2022 Anthony Paul Astolfi
#
# script/common.sh - Common Bash Script code.
#
if [ "$script_dir" == "" ]; then
    echo "Set script_dir before including this script!"
    exit 1
fi

project_dir=$(cd ${script_dir}/.. && pwd)
local_conan_parent_dir=${project_dir}/
local_conan_dir=${local_conan_parent_dir}/.conan
default_conan_dir=${HOME}/.conan

cd "$project_dir"

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# usage: verbose [stuff to print]...
#
# Prints arguments like `echo`, iff VERBOSE=1
#
function verbose {
    if [[ "$VERBOSE" == "1" ]]; then
        echo "($@)" >&2
    fi
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# usage: parse_version MAJOR.MINOR.PATCH[-devel] PART_NUM
#
# Parse semantic version string, printing the specified part.
#
# Examples:
#
#  COMMAND: parse_version 0.4.7 1
#  OUTPUT:  0
#
#  COMMAND: parse_version 0.4.7 2
#  OUTPUT:  4
#
#  COMMAND: parse_version 0.4.7 3
#  OUTPUT:  7
#
#  COMMAND: parse_version 0.4.7 4
#  OUTPUT:
#
#  COMMAND: parse_version 0.4.7-devel 4
#  OUTPUT:  devel
#
function parse_version {
    version=$1
    part=$2
    echo "${version}" | sed -e 's,[.-], ,g' | awk "{ print \$${part}; }"
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# usage: find_next_version MAJOR.MINOR.PATCH "major"|"minor"|"patch"
#
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
        echo "usage: $0 [major|minor|patch](default=patch)" >&2
        exit 1
    fi
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Exit with non-0 status if the git working tree is not clean.
#
function working_tree_is_clean {
    if [[ -n $(git status --short) ]]; then
        false
    fi
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Print the most recent release tag on the current branch.
#
function find_release_tag {
    git tag --list --merged HEAD --sort=-version:refname release-* | head -1
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Print the release version given a release tag.
#
function version_from_release_tag {
    release_tag=$1
    echo "${release_tag}" | sed -e 's,release-,,g'
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Exit with non-0 status if the specified env var isn't defined.
#
function require_env_var {
    var_name=$1
    if [ "${!var_name}" = "" ]; then
        echo "Error: env var '${var_name}' not specified!" >&2
        exit 1
    fi
}
