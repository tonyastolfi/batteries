######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
# Copyright 2022-2023 Anthony Paul Astolfi
#
# script/conan-version.sh - Sets conan_available and conan_version_2
#
# This should be source-ed into another script, not run on its own.
#
{
    which conan >/dev/null
} && {
    conan_available=1
} || {
    conan_available=0
}

if [ "${conan_available}" == "1" ]; then
    conan_version_2=$({ conan --version | grep -i 'conan version 2' >/dev/null ; } && echo 1 || echo 0)
    if [ "${conan_version_2}" == "1" ]; then
        export CONAN_HOME=${CONAN_HOME:-${CONAN_USER_HOME:-${HOME}}/.conan2}
        export CONAN_USER_HOME=${CONAN_USER_HOME:-${CONAN_HOME}/..}
    else
        export CONAN_USER_HOME=${CONAN_USER_HOME:-${HOME}}
    fi
fi
