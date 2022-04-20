######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
# Copyright 2022 Anthony Paul Astolfi
#
# script/common.sh - Common Bash Script code.
#
if [ "$script_dir" == "" ]; then
    echo $(cat <<EOF
           Set script_dir before including this script!
           (example: 'script_dir=\$(cd \$(dirname \$0) && pwd)')
EOF
        ) >&2
    return 1
fi

# Determine the project root directory.  First attempt to find the
# local git top-level dir relative to the parent directory of this
# file; if this fails, then use the parent of the script dir.
#
function find_project_dir() {
    git rev-parse --show-toplevel || { cd ${script_dir}/.. && pwd; }
}

project_dir=$(find_project_dir)
local_conan_parent_dir=${project_dir}/
local_conan_dir=${local_conan_parent_dir}/.conan
default_conan_dir=${HOME}/.conan

cd "$project_dir"

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# usage: verbose [stuff to print]...
#
# Prints arguments like `echo`, iff VERBOSE=1
#
function verbose() {
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
function parse_version() {
    local version=$1
    local part=$2
    echo "${version}" | sed -e 's,[.-], ,g' | awk "{ print \$${part}; }"
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# usage: find_next_version MAJOR.MINOR.PATCH "major"|"minor"|"patch"
#
function find_next_version() {
    local current_version=$1
    local release_type=$2
    local major_num=$(parse_version "${current_version}" 1)
    local minor_num=$(parse_version "${current_version}" 2)
    local patch_num=$(parse_version "${current_version}" 3)
    local devel_tag=$(parse_version "${current_version}" 4)

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
        return 1
    fi
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Return non-0 status if the git working tree is not clean.
#
function working_tree_is_clean() {
    if [[ -n $(git status --short) ]]; then
        return 1
    fi
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Print the most recent release tag on the current branch.
#
function find_release_tag() {
    local current_release=${OVERRIDE_RELEASE_TAG:-$(git tag --list \
                                                        --merged HEAD \
                                                        --sort=-version:refname \
                                                        release-* | head -1)}
    if [ "${current_release}" != "" ]; then
        echo $current_release
    else
        echo 'release-0.0.0'
    fi
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Print the SHA hash for the given GIT ref.
#
function find_git_hash() {
    git rev-list -n 1 "${1:-HEAD}" | awk '{print $1}'
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Print the SHA hash for the given release tag.
# If OVERRIDE_RELEASE_TAG is set, then prints the SHA hash of HEAD.
#
function find_release_commit_hash() {
    if [ "${OVERRIDE_RELEASE_TAG}" != "" ]; then
        find_git_hash "HEAD"
    else
        find_git_hash "${latest_release_tag}"
    fi
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Print the release version given a release tag.
#
function version_from_release_tag() {
    local release_tag=$1
    echo "${release_tag}" | sed -e 's,release-,,g'
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Return non-0 status if the specified env var isn't defined.
#
function require_env_var() {
    local var_name=$1
    if [ "${!var_name}" = "" ]; then
        echo "Error: env var '${var_name}' not specified!" >&2
        return 1
    fi
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- - - - -
# Find a given filename by searching the current working directory and
# going up the ancestor chain until we find something or hit root.
#
function find_parent_dir() {
  local anchor_file_name="$1"
  local dir="${2:-$(pwd)}"

  # Stop when we find '$anchor_file_name'
  #
  test -e "${dir}/${anchor_file_name}" && echo "${dir}" && return 0

  # If we've reached the root with no match, fail.
  #
  [ '/' = "${dir}" ] && return 1

  # Recurse up the parent chain.
  #
  find_parent_dir "${anchor_file_name}" "$(dirname "${dir}")"
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Print the current conan project dir.
#
function find_conan_dir() {
    find_parent_dir 'conanfile.py'
}

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Print the current conan version.
#
function find_conan_version() {
    NO_CHECK_CONAN=1 conan inspect --raw 'version' "$(find_conan_dir)"
}
