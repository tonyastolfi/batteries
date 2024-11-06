#!/bin/bash
#
# Copyright 2024 Anthony Paul Astolfi
#
set -Eeuo pipefail
if [ "${DEBUG:-0}" == "1" ]; then
    set -x
fi

tools_dir="$(cd "$(dirname "$0")" && realpath .)"
source "${tools_dir}/common.sh"

tmp_dir=$(mktemp -d)
if [ "${KEEP:-0}" == "0" ]; then
    trap "rm -rf \"${tmp_dir}\"" EXIT
else
    trap "echo \"Not removing temp dir (KEEP=1); cd ${tmp_dir}\"" EXIT
fi

user_id=$(id -u)
user_name=$(id -un)
group_id=$(id -g)
group_name=$(id -gn)
user_home_dir="$(cd && pwd)"

cat > "${tmp_dir}/Dockerfile" <<EOF
# syntax=docker/dockerfile:1

FROM $("${tools_dir}/docker-ci-image.sh")

# Add user/group within container.
#
RUN groupadd -g "${group_id}" "${group_name}"
RUN useradd --home-dir "${user_home_dir}" --no-create-home --uid "${user_id}" --gid "${group_id}" "${user_name}"

# --shell /bin/bash

# Set the active user.
#
USER "${user_name}"
EOF

user_image_hash=$(cat "${tmp_dir}/Dockerfile" | sha256sum | awk '{print $1}')
user_image_id="batt-user-image_${user_name}_${user_image_hash}"

if [ "$(docker image inspect -f json "${user_image_id}" 2>/dev/null)" == "[]" ]; then
    cd "${tmp_dir}"
    bash -c "docker build -t \"${user_image_id}\" . 2>&1" >"${tmp_dir}/docker.out" \
        || {
        cat "${tmp_dir}/docker.out"
        false
    }
fi

echo "${user_image_id}"
