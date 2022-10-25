#!/bin/bash
#
# Copyright 2022 Anthony Paul Astolfi
#
script_dir=$(dirname "$0")
PYTHONPATH=${script_dir}:${PYTHON_PATH} python3 -c 'import batt; batt.run_like_main(batt.get_version)'
