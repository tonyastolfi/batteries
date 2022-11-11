#!/bin/bash
#
# Copyright (C) 2022 Anthony Paul Astolfi
#

project_dir=$(git rev-parse --show-toplevel)

if [ -f "${project_dir}/_batt-docker-image" ]; then
    BATT_DOCKER_IMAGE=$(cat "${project_dir}/_batt-docker-image")
fi

docker_image=${BATT_DOCKER_IMAGE:-registry.gitlab.com/batteriescpp/batteries:latest.linux_gcc11_amd64}

# Figure out if the current shell is a TTY.
#
if [ -t 0 ]; then
    DOCKER_FLAGS_INTERACTIVE=-it
else
    DOCKER_FLAGS_INTERACTIVE=
fi

# Run the passed arguments as a shell command in a fresh docker
# container based on our CI image.
#
docker run ${DOCKER_FLAGS_INTERACTIVE} \
       --ulimit memlock=-1:-1 \
       -v /etc/passwd:/etc/passwd \
       -v /etc/group:/etc/group \
       --user $(id -u):$(id -g) \
       --network host \
       -v "$(pwd)":"$(pwd)" \
       -v "$HOME/.conan":"$HOME/.conan" \
       -w "$(pwd)" \
       ${docker_image} \
       "{ test -f ${project_dir}/_batt-docker-profile && source ${project_dir}/_batt-docker-profile || true } && $@"
