#/bin/bash
#
# Copyright 2022 Anthony Paul Astolfi
#
# script/self-check.sh - Run unit tests for all the scripts in this directory
#
set -Eeuo pipefail

script_dir=$(cd $(dirname $0) && pwd)

function assert_unbound_variable {
    local name=$1
    bash -uc "echo \"\${${name}}\"" 2>/dev/null 1>/dev/null && {
        echo "Failure: expected variable '${name}' to be unbound, actual=${!name}" >&2
        exit 1
    }
    true
}

function assert_streq {
    local left=$1
    local right=$2
    if [ "${left}" != "${right}" ]; then
        echo "Failure: expected '${left}' to equal '${right}'" >&2
        exit 1
    fi
    true
}

function assert_status {
    local stdout_temp=$(mktemp)
    exec 3<> $stdout_temp
    unlink $stdout_temp
    
    local stderr_temp=$(mktemp)
    exec 4<> $stderr_temp
    unlink $stderr_temp

    local expected_status=$1
    
    bash -c "${*:2}" 1>/dev/fd/3 2>/dev/fd/4
    
    local actual_status=$?
    local actual_stdout=$(cat<&3)
    local actual_stderr=$(cat<&4)
    
    if [ "$expected_status" != "$actual_status" ]; then
        echo "Failure: expected status=$expected_status, got $actual_status" >&2
        failed=1
    fi

    exec 3>&-
    exec 4>&-

    failed=0
    if [ "${EXPECT_STDOUT:-${actual_stdout}}" != "${actual_stdout}" ]; then
        echo -e "Failure: expected STDOUT:\n---\n${EXPECT_STDOUT}\n---" >&2
        failed=1
    fi
    if [ "${EXPECT_STDERR:-${actual_stderr}}" != "${actual_stderr}" ]; then       
        echo -e "Failure: expected STDERR:\n---\n${EXPECT_STDERR}\n---" >&2
        failed=1
    fi
    if [ $failed -eq 1 ]; then
        echo "command: ${@:2}"
        echo -e "stdout:\n---\n$actual_stdout\n---"
        echo -e "stderr:\n---\n$actual_stderr\n---"
        exit 1
    fi
}

(
    set -Eeuo pipefail

    #==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
    #
    echo "Testing conan-login.sh..."
    
    assert_unbound_variable CONAN_PASSWORD
    assert_unbound_variable CONAN_LOGIN_USERNAME

    EXPECT_STDOUT="" \
    EXPECT_STDERR="Error: env var 'RELEASE_CONAN_REMOTE' not specified!" \
    assert_status 1 "${script_dir}/conan-login.sh"
    
    assert_unbound_variable CONAN_PASSWORD
    assert_unbound_variable CONAN_LOGIN_USERNAME

    EXPECT_STDOUT="NO CI CREDENTIALS FOUND - not running 'conan user' command." \
    EXPECT_STDERR="" \
    assert_status 0 RELEASE_CONAN_REMOTE=gitlab "${script_dir}/conan-login.sh"
    
    assert_unbound_variable CONAN_PASSWORD
    assert_unbound_variable CONAN_LOGIN_USERNAME
    
) && {
    echo "PASSED."
} || {
    echo "FAILED."
    exit 1
}

