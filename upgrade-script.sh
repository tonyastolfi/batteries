#!/bin/bash
#
# Copyright 2023 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

script_dir=$(cd $(dirname $0) && pwd)
script_name=$(basename $0)

# Use the name of the current script to figure out if the submodule is
# `script` or `tools`.
#
if [ "${script_name}" == "upgrade-script.sh" ]; then
    submodule_relpath=script
elif [ "${script_name}" == "upgrade-tools.sh" ]; then
    submodule_relpath=tools
else
    echo "Could not figure out what this script is! ${script_name}" >&2
    exit 1
fi

# The first arg is the project directory; if not specified, try to
# find the git top-level directory.
#
project_dir=${1:-$(git rev-parse --show-toplevel)}
cd "${project_dir}"
project_dir=$(pwd)

source "${script_dir}/common.sh"

project_is_dirty=$(working_tree_is_clean && echo "0" || echo "1")

# Make sure that the a8 submodule has been initialized.
#
echo "------ Initializing submodules..."
git submodule update --init --quiet
echo "OK"
echo

# Calculate whether we are up to date.
#
old_commit=$(git submodule status | grep "${submodule_relpath}" | awk '{print $1}' | sed -E 's,[^0-9a-fA-F],,g')

echo "------ Fetching remote branches for submodule '${submodule_relpath}'..."
cd "${project_dir}/${submodule_relpath}"
git fetch origin

current_commit=$(find_git_hash HEAD)
latest_commit=$(find_git_hash origin/script)

if [ "${old_commit}" != "${current_commit}" ]; then
    echo $(cat <<EOF
           Error: Sanity check failed!  Different commit hash values for git
           submodule and find_git_hash.
EOF
        ) >&2
    exit 1
fi

needs_update=0
if [ "$current_commit" != "$latest_commit" ]; then
    echo "${current_commit} => ${latest_commit}"
    needs_update=1
fi
echo "OK"
echo

cd "${project_dir}"

# Always update the submodule from remote if not dry run.
#
if [ "${DRY_RUN:-0}" == "1" ]; then
    echo "------ Skipping git submodule update (because DRY_RUN==1)"
    echo
else
    echo "------ Running git submodule update..."
    git submodule update --remote
    new_commit=$(git submodule status | grep "${submodule_relpath}" | awk '{print $1}' | sed -E 's,[^0-9a-fA-F],,g')
    echo "OK"
    echo

    if [ "${new_commit}" != "${latest_commit}" ]; then
        echo $(cat <<EOF
               Error: Sanity check failed!  Different commit hash values for git
               submodule and find_git_hash.
EOF
            ) >&2
        exit 1
    fi
fi

# Perform actions or print messages, depending on DRY_RUN.
#
echo "------ Committing changes..."
if [ "${DRY_RUN:-0}" == "1" ]; then
    if [ "${project_is_dirty}" == "0" ]; then
        echo "(Working dir clean; would update project, but DRY_RUN==1)"
    else
        if [ "${needs_update}" == "1" ]; then
            echo "Warning: would not commit submodule change; working tree is dirty"
        fi
    fi
    if [ "${needs_update}" == "1" ]; then
        echo "(No action taken; re-run without DRY_RUN=1 to apply changes)"
    fi
else
    if [ "${needs_update}" == "0" ]; then
        echo "(Already up-to-date)"
    else
        if [ "${project_is_dirty}" == "0" ]; then
            git add ${submodule_relpath}
            git commit -m "Upgrade ${submodule_relpath} submodule to '${latest_commit}' (performed by upgrade-script.sh)."
        else
            echo "Warning: not committing submodule change; working tree is dirty"
        fi
        echo "Successfully upgraded ${submodule_relpath} submodule to latest version."
    fi
fi
echo "OK"
echo
