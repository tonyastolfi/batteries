#!/bin/bash
#
# Copyright 2022 Anthony Paul Astolfi
#
cd "$(dirname "$0")"
python3 -c 'import batt; batt.run_like_main(batt.get_version)'
