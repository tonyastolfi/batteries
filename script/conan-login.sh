######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
# Copyright 2022 Anthony Paul Astolfi
#
# script/conan-login.sh - Log in to Conan Package Repo.
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
require_env_var RELEASE_CONAN_REMOTE

# Apply Conan package registry credentials.
#
if [ "${conan_login}" != "" ] && [ "${conan_pass}" != "" ]; then
    echo "conan user --remote=${RELEASE_CONAN_REMOTE} --password=\"${conan_pass}\" ${conan_login} ;"
fi

echo "export CONAN_PASSWORD=\"${conan_pass}\" ;"
echo "export CONAN_LOGIN_USERNAME=\"${conan_login}\" ;"
