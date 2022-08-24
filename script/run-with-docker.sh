#!/bin/bash
#
# Copyright (C) 2022 Anthony Paul Astolfi
#

# Run the passed arguments as a shell command in a fresh docker
# container based on our CI image.
#
docker run -it \
       -v /etc/passwd:/etc/passwd \
       -v /etc/group:/etc/group \
       --user $(id -u):$(id -g) \
       --network host \
       -v "$(pwd)":"$(pwd)" \
       -v "$HOME/.conan":"$HOME/.conan" \
       -w "$(pwd)" \
       registry.gitlab.com/tonyastolfi/batteries${BATT_DOCKER_IMAGE_EXT}:latest \
       "$@"
