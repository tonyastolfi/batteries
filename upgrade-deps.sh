#!/bin/bash
#
# Copyright 2022-2023 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

script_dir=$(cd $(dirname $0) && pwd)

project_dir=${1:-$(git rev-parse --show-toplevel)}
project_name=$(conan inspect "${project_dir}" --raw=name)

test -f "${project_dir}/conanfile.py" || exit 1

cd "${project_dir}"
project_dir=$(pwd)

source "${script_dir}/common.sh"

project_is_dirty=$(working_tree_is_clean && echo "0" || echo "1")

# Extract the required dependencies from the project via conan inspect
# as a bash list.
#
#  conan graph info -vquiet -f json . | jq '.nodes[0]|.requires|to_entries|map(.value)'
if [ "${conan_version_2}" == "1" ]; then
    all_deps=($(conan inspect --raw=requires "${project_dir}" \
                    | sed -e "s,',\",g" \
                    | jq -r '.[]'))
else
    all_deps=($(conan inspect --raw=requires "${project_dir}" \
                    | sed -e "s,',\",g" \
                    | jq -r '.[]'))
fi

up_to_date=1

# For each of the required dependencies of the project...
#
for dep in "${all_deps[@]}"
do
    # Take the full dependency ref up to the first '/' character to
    # extract the conan package name.
    #
    dep_name=$(echo "${dep}" | sed -e 's,/.*,,g')
    echo "(Checking ${dep_name}...)" >&2

    if [ "${EXCLUDE:-}" == "" ] || \
           ! { echo "$dep_name" | grep -E "^${EXCLUDE:-}\$" >/dev/null; } then
        latest_version=$("${script_dir}/show-latest.sh" "${dep_name}")
        if [ "${latest_version}" == "" ]; then
            exit 1
        fi
        current_version="$dep"

        verbose "latest=${latest_version}, current=${current_version}"

        test -x "${script_dir}/version-compare.sh" || {
            echo "Error: could not find script/version-compare.sh!" >&2
            exit 1
        }
        
        "${script_dir}/version-compare.sh" \
            "${latest_version}" \
            "${current_version}" || {

            up_to_date=0
            if [ "${DRY_RUN:-0}" == "1" ]; then
                echo "${current_version} => ${latest_version} (No action taken because DRY_RUN==1)"
            else
                echo "${current_version} => ${latest_version} (Updating conanfile.py...)"
                replace_in_file \
                    -e "s,${current_version},${latest_version},g" \
                    "${project_dir}/conanfile.py"
            fi
        }
    elif [ "${EXCLUDE:-}" != "" ]; then
        echo "(ignoring \"${dep_name}\" because of EXCLUDE pattern)"
    fi
done

if [ "${up_to_date}" == "1" ]; then
    echo "(All dependencies are up-to-date)"
else
    if [ "${DRY_RUN:-0}" == "1" ]; then
        if [ "${project_is_dirty}" == "0" ]; then
            echo "(Upgrades available; re-run without DRY_RUN=1 to commit changes)"
        else
            echo "Warning: will not commit upgrades; the working tree is dirty:"
            git status -s
            echo "(Upgrades available; re-run without DRY_RUN=1 to modify working files)"
        fi
    else
        if [ "${project_is_dirty}" == "0" ]; then
            git add conanfile.py
            git commit -m 'Upgraded dependencies to latest versions (performed by a8/script/upgrade-deps.sh)'
        else
            echo "Warning: did not commit; the working tree was dirty:"
            git status -s
        fi
    fi
fi
