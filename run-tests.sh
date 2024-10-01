#!/bin/bash
#
set -Eeuo pipefail

script_dir="$(cd $(dirname $0) && pwd)"
source "${script_dir}/common.sh"

require_env_var BUILD_TYPE
require_env_var project_dir

build_dir="${BUILD_DIR:-${project_dir}/build/${BUILD_TYPE}}"

mkdir -p "${build_dir}"
cd "${build_dir}"
build_dir="$(realpath .)"

function run_test() {
    test_exe="$1"
    
    if [ "${GTEST_FILTER:-}" == "" ]; then
	echo -e "\n\nRunning DEATH tests ==============================================\n"
	GTEST_OUTPUT="xml:${build_dir}/death-test-results.xml" GTEST_FILTER='*Death*' "${test_exe}"
        
	echo -e "\n\nRunning non-DEATH tests ==========================================\n"
        GTEST_OUTPUT="xml:${build_dir}/test-results.xml" GTEST_FILTER='*-*Death*' "${test_exe}"
        
    else
	GTEST_OUTPUT="xml:${build_dir}/test-results.xml" "${test_exe}"
    fi
    
}

os_name=$(uname -s)
if [ "${os_name}" == "Darwin" ]; then
    with_execute_permission="-perm +0111"
else
    with_execute_permission="-perm /111"
fi

for name in $(find "${project_dir}/build/${BUILD_TYPE}" -type f ${with_execute_permission} -name '*Test');
do
    run_test "${name}"
done
