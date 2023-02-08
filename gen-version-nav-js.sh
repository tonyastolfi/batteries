#!/bin/bash
#
set -e

deploy_repo_dir=$1

echo 'var releaseNavOptions = ['
echo ' "latest",'
ls "${deploy_repo_dir}" \
    | grep -E 'v[0-9]+(\.[0-9])*(-devel)?' \
    | sort --version-sort --reverse \
    | xargs -n 1 -I {} \
            echo " \"{}\","

echo '];'
