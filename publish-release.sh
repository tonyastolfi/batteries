#!/bin/bash -x
#
# Copyright 2022 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

echo "pwd=$(pwd)"

script_dir=$(cd $(dirname $0) && pwd)
source "${script_dir}/common.sh"
source "${script_dir}/conan-login.sh"

echo "project_dir is '${project_dir}'"

# Verify all required variables are defined.
#
require_env_var RELEASE_CONAN_REMOTE

if [ "${conan_version_2}" == "1" ]; then
    default_conan_recipe_user=
    default_conan_recipe_channel=
    conan_upload_extra_args=
else
    default_conan_recipe_user=_
    default_conan_recipe_channel=_
    conan_upload_extra_args=--all
fi

conan_recipe_user=${RELEASE_CONAN_USER:-${default_conan_recipe_user}}
conan_recipe_channel=${RELEASE_CONAN_CHANNEL:-${default_conan_recipe_channel}}

# The working tree must be clean before we continue...
#
working_tree_is_clean || {
    echo $(cat <<EOF
           Error: the working tree has uncommitted changes; please commit all
           and retry.
EOF
        ) >&2
    exit 1;
}

# Make sure the latest commit has a release tag.
#
active_version=$("${script_dir}/get-version.sh")
release_version=$("${script_dir}/get-release.sh")

if [ "$active_version" != "$release_version" ]; then
    echo $(cat <<EOF
           Error: active version (${active_version}) does not match the release
           version (${release_version}); did you forget to run
           ${script_dir}/tag-release.sh?
EOF
        ) >&2
    exit 1
fi

if [ "${CI_COMMIT_TAG:-}" != "" ]; then
    if [ "${CI_COMMIT_TAG:-}" != "release-${release_version}" ]; then
        echo $(cat <<EOF
               Warning: the active version (${active_version}) does not match
               the GitLab CI_COMMIT_TAG (${CI_COMMIT_TAG})!
EOF
            ) >&2
    fi
fi

conan_name=$(find_conan_project_name)
conan_recipe=${conan_name}/${release_version}@${conan_recipe_user}/${conan_recipe_channel}
conan_build_type=${BUILD_TYPE:-Release}
conan_build_dir=${project_dir}/build/${conan_build_type}
conan_config_flags=$("${script_dir}/conan-config-flags.sh")

if [ "${conan_version_2}" == "1" ]; then
    # Remove trailing '@/' if conan 2.
    #
    conan_recipe=$(echo "${conan_recipe}" | sed -E 's,@/$,,g')

    # Set the create/export args (Conan 2.x)
    #
    conan_export_args="--user=${conan_recipe_user} --channel=${conan_recipe_channel} ."
    conan_create_args="--user=${conan_recipe_user} --channel=${conan_recipe_channel} ../.."
else
    # Set the create/export args (Conan 1.x)
    #
    conan_export_args=". ${conan_recipe}"
    conan_create_args="../.. ${conan_recipe}"
fi

verbose "Publishing ${conan_recipe}..."

if [ "${EXPORT_PKG_ONLY:-0}" == "1" ]; then
    conan_export_command="( cd \"${project_dir}\" && conan export ${conan_export_args} )"
    conan_export_pkg_command="( cd \"${project_dir}\" && conan export-pkg ${conan_config_flags} ${conan_export_args} )"

    echo "${conan_export_command}"
    bash -c "${conan_export_command}"

    echo "${conan_export_pkg_command}"
    bash -c "${conan_export_pkg_command}"
else
    # This is just a sanity check; the project must be fully built before
    # we continue.
    #
    ( cd "${project_dir}" && make BUILD_TYPE=${conan_build_type} install )

    # Create the release package...
    #
    conan_create_command="cd \"${conan_build_dir}\" && conan create ${conan_config_flags} ${conan_create_args}"
    echo "${conan_create_command}"
    bash -c "${conan_create_command}"
fi

# ...and upload it!
#
CONAN_PASSWORD=${conan_pass}        \
CONAN_LOGIN_USERNAME=${conan_login} \
  conan upload ${conan_recipe} --confirm ${conan_upload_extra_args} --remote=${RELEASE_CONAN_REMOTE}
