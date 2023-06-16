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
require_env_var RELEASE_CONAN_CHANNEL
require_env_var RELEASE_CONAN_REMOTE

# If RELEASE_CONAN_USER is not defined, then attempt to default to the GitLab
# CI_PROJECT_NAMESPACE and CI_PROJECT_TITLE; if these too are not defined,
# then show the error requiring RELEASE_CONAN_USER.
#
conan_recipe_user=${RELEASE_CONAN_USER:-"${CI_PROJECT_NAMESPACE:-}+${CI_PROJECT_TITLE:-}"}
if [ "$conan_recipe_user" == "+" ]; then
    require_env_var RELEASE_CONAN_USER
    conan_recipe_user=${RELEASE_CONAN_USER}
fi

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

conan_name=$(conan inspect --raw=name "${project_dir}")
conan_recipe=${conan_name}/${release_version}@${conan_recipe_user}/${RELEASE_CONAN_CHANNEL}
conan_build_type=${BUILD_TYPE:-Release}
conan_build_dir=${project_dir}/build/${conan_build_type}

verbose "Publishing ${conan_recipe}..."

# This is just a sanity check; the project must be fully built before
# we continue.
#
( cd "${project_dir}" && make BUILD_TYPE=${conan_build_type} install )

# Create the release package...
#
conan_config_flags=$("${script_dir}/conan-config-flags.sh")
conan_create_command="cd \"${conan_build_dir}\" && conan create ${conan_config_flags}  ../.. ${conan_recipe}"
echo "${conan_create_command}"
bash -c "${conan_create_command}"

# ...and upload it!
#
CONAN_PASSWORD=${conan_pass}        \
CONAN_LOGIN_USERNAME=${conan_login} \
  conan upload ${conan_recipe} --confirm --all --remote=${RELEASE_CONAN_REMOTE}
