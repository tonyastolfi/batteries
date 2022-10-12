#!/bin/bash
#
# Copyright (C) 2022 Anthony Paul Astolfi
#

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
       -v /etc/passwd:/etc/passwd \
       -v /etc/group:/etc/group \
       --user $(id -u):$(id -g) \
       --network host \
       -v "$(pwd)":"$(pwd)" \
       -v "$HOME/.conan":"$HOME/.conan" \
       -w "$(pwd)" \
       registry.gitlab.com/batteriescpp/batteries:latest \
       "$@"
