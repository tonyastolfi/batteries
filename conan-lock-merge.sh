#!/bin/bash
#
# conan-lock-merge.sh SETTINGS...
#
# Creates a Conan lockfile with the passed settings and merges it with
# ${project_dir}/conan.lock.  If the conan.lock file does not already
# exist, the newly generated one becomes the new conan.lock.
#
# Example:
#
# conan-lock-merge.sh -s os=Linux -s arch=x86_64
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

script_dir="$(cd "$(dirname "$0")" && pwd)"

source "${script_dir}/common.sh"

settings="$@"

conan lock create "${project_dir}" --lockfile="" --lockfile-out='tmp.conan.lock' ${settings}

if [ -e "${project_dir}/conan.lock" ]; then
    conan lock merge --lockfile="${project_dir}/conan.lock" --lockfile='tmp.conan.lock' --lockfile-out="${project_dir}/conan.lock"
    rm 'tmp.conan.lock'
else
    mv 'tmp.conan.lock' "${project_dir}/conan.lock"
fi
