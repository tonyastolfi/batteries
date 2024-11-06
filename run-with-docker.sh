#!/bin/bash
#
# Copyright (C) 2022-2024 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-0}" == "1" ]; then
    set -x
fi

tools_dir="$(cd "$(dirname "$0")" && realpath .)"
source "${tools_dir}/common.sh"

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Calculate the Docker image to use.
#
docker_image=$("${tools_dir}/docker-user-image.sh")

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Figure out if the current shell is a TTY.
#
if [ -t 0 ]; then
    docker_flags_interactive=-it
else
    docker_flags_interactive=
fi

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Calculate volume mappings.
#
volume_mappings=()

# Add HOME (if defined).
#
if [ "${HOME:-}" != "" ] && [ -e "${HOME:-}" ]; then
    volume_mappings+=(-v "${HOME}":"${HOME}")
fi

# Add the current real path.
#
real_pwd=$(realpath "$(pwd)")
if [ -e "${real_pwd}" ]; then
    volume_mappings+=(-v "${real_pwd}":"${real_pwd}")
fi

# Add /local
#
if [ -e "/local" ]; then
    volume_mappings+=(-v "/local":"/local")
fi

# If the conan home dir is linked, map the linked location.
#
conan_home=${CONAN_HOME:-${HOME}/.conan2}
if [ -e "${conan_home}" ]; then
    {
        real_conan_home=$(cd "${conan_home}" && dirname "$(realpath .)")
        volume_mappings+=(-v "${real_conan_home}":"${real_conan_home}")
    } || true
    {
        real_conan_parent=$(cd "${conan_home}" && dirname "$(realpath ..)")
        if [ "${HOME}" != "${real_conan_parent}" ]; then
            volume_mappings+=(-v "${real_conan_parent}":"${real_conan_parent}")
        fi
    } || true
fi

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Capture the current environment.
#
docker_env=$(env | { grep -Ei 'release' || true ; } | xargs -I {} echo '--env' {})

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Run docker!
#
docker run \
       --ulimit memlock=-1:-1 \
       --cap-add SYS_ADMIN --device /dev/fuse \
       --privileged \
       "${volume_mappings[@]}" \
       -w "${real_pwd}" \
       ${docker_flags_interactive} \
       --rm \
       ${docker_env} \
       ${DOCKER_FLAGS:-} \
       ${EXTRA_DOCKER_FLAGS:-} \
       "${docker_image}" \
       bash -c "$*"
