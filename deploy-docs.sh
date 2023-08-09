#!/bin/bash -x
#
# Copyright 2022 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

script_dir=$(cd $(dirname $0) && pwd)
source "${script_dir}/common.sh"

# Get the current package version.
#
package_version=$(cd "${script_dir}/.." && "${script_dir}/get-version.sh")
echo "The current package version is ${package_version}"

require_env_var GITHUB_PAGES_ACCESS_TOKEN

site_dir=$1
deploy_dir=$2

# Build the deploy directory from scratch.
#
rm -rf "${deploy_dir}"
mkdir -p "${deploy_dir}"

# Clone the github pages site.
#
github_user=${GITHUB_PAGES_USER:-batteriescpp}
github_repo=${GITHUB_PAGES_REPO:-batteriescpp.github.io}

cd "${deploy_dir}"
git clone "https://${github_user}:${GITHUB_PAGES_ACCESS_TOKEN}@github.com/${github_user}/${github_repo}"

if [ "${CLONE_ONLY:-}" == "1" ]; then
    exit 0
fi

cd "${github_repo}"
git config user.email "batteriescpp@gmail.com"
git config user.name "${github_user}"

deploy_repo_dir=${deploy_dir}/${github_repo}

# Make sure there is a '.nojekyll' file present.
#
touch "${deploy_repo_dir}/.nojekyll"

# Remove previous docsite files and copy the new content.
#
rm -rf "${deploy_repo_dir}/v${package_version}"
mkdir -p "${deploy_repo_dir}/v${package_version}"

# Refresh symlinks
#
rm -f "${deploy_repo_dir}/latest"
ln -s "v${package_version}" "${deploy_repo_dir}/latest"

# Copy the generated site files to the github pages repo.
#
cp -aT "${site_dir}" "${deploy_repo_dir}/v${package_version}"

# Generate the doc version nav selector.
#
rm -f "${deploy_repo_dir}/releaseNavOptions.js"
"${script_dir}/gen-version-nav-js.sh" "${deploy_repo_dir}" > "${deploy_repo_dir}/releaseNavOptions.js"

# Commit changes.
#
cd "${deploy_repo_dir}"
git add **
git status
git commit -m 'Automatic deployment of docs.'
git status

# Go live!
#
git push
