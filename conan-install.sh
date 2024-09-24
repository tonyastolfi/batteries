#!/bin/bash
#
set -Eeuo pipefail

set -x

script_dir=$(cd $(dirname $0) && pwd)
source "${script_dir}/common.sh"

# Save a list of the cached packages before and after calling conan install.
#
package_list_prefix=/tmp/_batteries_conan_cache.
package_list_before=${package_list_prefix}before
package_list_after=${package_list_prefix}after

${script_dir}/conan-cache-contents.sh >${package_list_before}

#+++++++++++-+-+--+----- --- -- -  -  -   -
conan install "$@"
#+++++++++++-+-+--+----- --- -- -  -  -   -

${script_dir}/conan-cache-contents.sh >${package_list_after}

# Find everything that was added to the cache.
#
newly_cached=$(diff ${package_list_before} ${package_list_after} \
                   | grep -E '^> +' \
                   | sed -E 's,^> +,,g' \
                   || true)

echo ""
echo "Newly Cached Packages:"
echo "${newly_cached}"
echo ""

# If a Conan remote and login credentials for cachine were specified via
# environment variables, *and* we detected newly added packages, then try
# to upload them to the remote now.
#
# If the remote already has a cached copy of a given package, it will be a
# no-op (we do *not* specify --force for conan upload).
#
if [ "${CACHE_CONAN_REMOTE:-}" != "" ] && \
   [ "${CACHE_CONAN_LOGIN_USERNAME:-}" != "" ] && \
   [ "${CACHE_CONAN_PASSWORD:-}" != "" ] && \
   [ "${newly_cached}" != "" ]; then
    
    echo "(uploading new packages to cache server)"

    export RELEASE_CONAN_REMOTE=${CACHE_CONAN_REMOTE}
    export RELEASE_CONAN_LOGIN_USERNAME=${CACHE_CONAN_LOGIN_USERNAME}
    export RELEASE_CONAN_PASSWORD=${CACHE_CONAN_PASSWORD}

    # Authenticate with the server.
    #
    source "${script_dir}/conan-login.sh"

    # Call conan upload for each new package, one at a time.
    #
    echo "${newly_cached}" \
        | xargs -n 1 -t \
                conan upload --confirm --remote=${RELEASE_CONAN_REMOTE}
    
else
    echo "(skipping upload to cache remote)"
fi
