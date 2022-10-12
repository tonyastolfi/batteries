#!/bin/bash -x
#
# Copyright 2022 Anthony Paul Astolfi
#
set -e

script_dir=$(cd $(dirname $0) && pwd)
source "${script_dir}/common.sh"

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

cd "${github_repo}"
git config user.email "batteriescpp@gmail.com"
git config user.name "${github_user}"

deploy_repo_dir=${deploy_dir}/${github_repo}

# Remove previous docsite files.
#
find "${deploy_repo_dir}" -mindepth 1 -maxdepth 1 \( ! -name '.git' -a ! -name 'LICENSE' -a ! -name '.nojekyll' -a ! -name '.' -a ! -name '..' \) \
     | xargs -t rm -rf 

# Make sure there is a '.nojekyll' file present.
#
touch "${deploy_repo_dir}/.nojekyll"

# Copy the generated site files to the github pages repo.
#
cp -aT "${site_dir}" "${deploy_repo_dir}"

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
