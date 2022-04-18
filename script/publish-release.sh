#!/bin/bash
#
# Copyright 2022 Anthony Paul Astolfi
#
set -e

script_dir=$(cd $(dirname $0) && pwd)
source "${script_dir}/common.sh"

# Use GitLab CI Deployment Token to authenticate against the Conan
# package registry, if possible.  Otherwise fall back on env vars
# RELEASE_CONAN_LOGIN_USERNAME, RELEASE_CONAN_PASSWORD.
#
conan_login=${CI_DEPLOY_USER:-${RELEASE_CONAN_LOGIN_USERNAME}}
conan_pass=${CI_DEPLOY_PASSWORD:-${RELEASE_CONAN_PASSWORD}}

# Verify all required variables are defined.
#
require_env_var RELEASE_CONAN_USER
require_env_var RELEASE_CONAN_CHANNEL
require_env_var RELEASE_CONAN_REMOTE

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

if [ "$CI_COMMIT_TAG" != "" ]; then
    if [ "$CI_COMMIT_TAG" != "release-${release_version}" ]; then
        echo $(cat <<EOF
               Warning: the active version (${active_version}) does not match
               the GitLab CI_COMMIT_TAG (${CI_COMMIT_TAG})!
EOF
            ) >&2
    fi
fi

conan_recipe=batteries/${release_version}@${RELEASE_CONAN_USER}/${RELEASE_CONAN_CHANNEL}
verbose "Publishing ${conan_recipe}..."

# Apply Conan package registry credentials.
#
if [ "${conan_login}${conan_pass}" != "" ]; then
    echo "Setting Conan user credentials..." >&2
    conan user --remote=${RELEASE_CONAN_REMOTE} --password=${conan_pass} ${conan_login}
fi

# This is just a sanity check; the project must be fully built before
# we continue.
#
( cd ${project_dir} && make BUILD_TYPE=Release install build test create )

# Create the release package...
#
( cd ${project_dir}/build/Release && conan create ../.. ${conan_recipe} )

# ...and upload it!
#
CONAN_PASSWORD=${conan_pass}        \
CONAN_LOGIN_USERNAME=${conan_login} \
  conan upload --remote=${RELEASE_CONAN_REMOTE} ${conan_recipe}
