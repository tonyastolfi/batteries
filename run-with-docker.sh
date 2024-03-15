#!/bin/bash
#
# Copyright (C) 2022-2024 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-}" == "1" ]; then
    set -x
fi

script_dir=$(cd "$(dirname "$0")" && pwd)
source "${script_dir}/common.sh"

docker_image=${BATT_DOCKER_IMAGE:-$("${script_dir}/ci-docker-image.sh")}

# Figure out if the current shell is a TTY.
#
if [ -t 0 ]; then
    DOCKER_FLAGS_INTERACTIVE=-it
else
    DOCKER_FLAGS_INTERACTIVE=
fi

real_pwd=$(realpath $(pwd))

conan_version="$(docker run ${docker_image} conan --version | sed -E 's,.*[^0-9]([0-9]+\.[0-9]+\.[0-9]+).*,\1,g')"
conan2_dir="${HOME}/.conan_${conan_version}"

mkdir -p "${HOME}/.conan"
mkdir -p "${HOME}/.conan2"

DOCKER_ENV=$(env | { grep -Ei 'release|conan' || true ; } | xargs -I {} echo '--env' {})


# Run the passed arguments as a shell command in a fresh docker
# container based on our CI image.
#
docker run ${DOCKER_FLAGS_INTERACTIVE} \
       --ulimit memlock=-1:-1 \
       --cap-add SYS_ADMIN --device /dev/fuse \
       --privileged \
       -v /etc/passwd:/etc/passwd \
       -v /etc/group:/etc/group \
       --user $(id -u):$(id -g) \
       --network host \
       -v "$(pwd)":"$(pwd)" \
       -v "$real_pwd":"$real_pwd" \
       -v "$HOME/.cache":"$HOME/.cache" \
       -v "${conan2_dir}":"${conan2_dir}" \
       -w "$(pwd)" \
       ${DOCKER_ENV} \
       ${EXTRA_DOCKER_FLAGS:-} \
       ${docker_image} \
       bash -c "export CONAN_USER_HOME=${HOME} && export CONAN_HOME=${conan2_dir} && { test -f ${project_dir}/_batt-docker-profile && source ${project_dir}/_batt-docker-profile || true; } && $*"
