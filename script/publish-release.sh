#!/bin/bash
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

if [ "${RELEASE_CONAN_USER}" = "" ]; then
    echo "Error: env var 'RELEASE_CONAN_USER' not specified!" >&2
    exit 1
fi

if [ "${RELEASE_CONAN_CHANNEL}" = "" ]; then
    echo "Error: env var 'RELEASE_CONAN_CHANNEL' not specified!" >&2
    exit 1
fi

if [ "${RELEASE_CONAN_REMOTE}" = "" ]; then
    echo "Error: env var 'RELEASE_CONAN_REMOTE' not specified!" >&2
    exit 1
fi

working_tree_is_clean || {
    echo "Error: the working tree has uncommitted changes; please commit all and retry." >&2
    exit 1;
}

active_version=$("${script_dir}/get-version.sh")
release_version=$("${script_dir}/get-release.sh")

if [ "$active_version" != "$release_version" ]; then
    echo $(cat <<EOF
Error: active version (${active_version}) does not match the release version 
(${release_version}); did you forget to run ${script_dir}/tag-release.sh? 
EOF
        ) >&2
    exit 1
fi

if [ "${conan_login}${conan_pass}" != "" ]; then
    conan user --remote=${RELEASE_CONAN_REMOTE} --password=${conan_pass} ${conan_user}
fi

conan_recipe=batteries/${release_version}@${RELEASE_CONAN_USER}/${RELEASE_CONAN_CHANNEL}

( cd ${project_dir} && make BUILD_TYPE=Release install build test create )
( cd ${project_dir}/build/Release && conan create ../.. ${conan_recipe} )

conan upload --remote=${RELEASE_CONAN_REMOTE} ${conan_recipe}
