#!/bin/bash
#
set -e

script_dir=$(cd $(dirname $0) && pwd)
source "${script_dir}/common.sh"

version_from_release_tag $(find_release_tag)
