#
# Copyright 2022 Anthony Paul Astolfi
#
import os, sys, re


# Whether to emit verbose output; this can be overridden by client applications.
#
VERBOSE = os.getenv('VERBOSE') and True or False

# Detect whether we are on Conan version 2.
#
CONAN_VERSION_2 = (
    os.popen(
        '{ conan --version | grep -i "conan version 2" >/dev/null; } && echo 1 || echo 0'
    )).read().strip() == '1'

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def verbose(msg):
    if VERBOSE:
        print(msg, file=sys.stderr)


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def find_release_tag():
    '''
    Returns the latest release tag on the current git branch.
    '''
    override_release_tag = os.getenv('OVERRIDE_RELEASE_TAG')
    if override_release_tag:
        current_release = override_release_tag
    else:
        output = (os.popen('git tag --list --merged HEAD --sort=-version:refname release-*')
                  .read()
                  .strip())

        current_release = re.split('\r|\n', output, maxsplit=1)[0]

    if current_release != '':
        return current_release
    else:
        return 'release-0.0.0'


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def version_from_release_tag(tag):
    '''
    Translates a git release tag string to a semantic version string.
    '''
    return re.sub('release-', '', tag)


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def find_next_version(current_version, release_type='patch'):
    parts = re.split('\\.|-', current_version) + ['0', '0', '0', '']
    major, minor, patch, is_devel = parts[:4]
    major, minor, patch, is_devel = int(major), int(minor), int(patch), is_devel == 'devel'

    if release_type == 'major':
        major += 1
        minor, patch, is_devel = 0, 0, False
    elif release_type == 'minor':
        minor += 1
        patch, is_devel = 0, False
    elif release_type == 'patch':
        if is_devel:
            is_devel = False
        else:
            patch += 1
    else:
        raise RuntimeError(f'bad release type: {release_type}')

    return f'{major}.{minor}.{patch}{"-devel" if is_devel else ""}'


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def working_tree_is_clean():
    return os.popen('git status --short').read().strip() == ''


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def find_git_hash(refspec='HEAD'):
    return os.popen(f'git rev-list -n 1 "{refspec}"').read().strip().split(' ')[0]


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def find_release_commit_hash():
    override_release_tag = os.getenv('OVERRIDE_RELEASE_TAG')
    if override_release_tag:
        return find_git_hash('HEAD')
    else:
        return find_git_hash(find_release_tag())


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def find_parent_dir(sentinel_file, current_dir=os.path.abspath(os.getcwd())):
    if os.path.isfile(os.path.join(current_dir, sentinel_file)):
        return current_dir

    if current_dir == '':
        raise RuntimeError('Error: conanfile.py not found in current working tree')

    parent_dir = os.path.dirname(current_dir)
    if parent_dir == current_dir:
        raise RuntimeError('Error: conanfile.py not found in current working tree')

    return find_parent_dir(sentinel_file, parent_dir)


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def find_conan_dir():
    return find_parent_dir(sentinel_file='conanfile.py')


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def find_conan_version():
    if CONAN_VERSION_2:
        command = f'NO_CHECK_CONAN=1 conan inspect -f json "{find_conan_dir()}" | jq -r ".version"'
    else:
        command = f'NO_CHECK_CONAN=1 conan inspect --raw \'version\' "{find_conan_dir()}\"'
    return os.popen(command).read().strip()


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def get_version(no_check_conan = (os.getenv('NO_CHECK_CONAN') == '1')):
    # Search for the latest release tag reachable from the current branch.
    #
    latest_release_tag = find_release_tag()
    latest_release = version_from_release_tag(latest_release_tag)
    active_version = latest_release

    verbose(f"pwd={os.getcwd()}")

    def devel(version_str):
        return f'{find_next_version(version_str, "patch")}-devel'

    if latest_release == '0.0.0':
        verbose(f'No git release tag found; using 0.1.0-devel as the initial version')
        active_version = '0.1.0-devel'

    elif not working_tree_is_clean():
        # If the working tree is dirty, then show the next patch version with
        # "-devel" appended.
        #
        verbose(f'The working tree is dirty; adding \'+1-devel\' to latest release \'{latest_release}\'')
        active_version = devel(latest_release)
    else:
        # Get the latest commit hash and the commit hash of the latest release
        # tag.
        #
        latest_commit = find_git_hash('HEAD')
        latest_release_commit = find_release_commit_hash()

        # If the working tree is clean but our branch is ahead of the release
        # tag, then we also want to emit the devel version.
        #
        if latest_commit != latest_release_commit:
            verbose(f'HEAD is ahead of last release tag \'{latest_release_tag}\'; ' +
                    f'adding \'+1-devel\' to latest release \'{latest_release}\'')
            active_version = devel(latest_release)

    if not no_check_conan:
        conan_version = find_conan_version()
        if conan_version != active_version:
            raise RuntimeError(f'Error: conan version ({conan_version}) does not match inferred ' +
                   f'version from git ({active_version})!  Please resolve this issue' +
                   ' and try again.')

    # Success!
    #
    return active_version


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def generate_conan_find_requirements(self):
    """
    Generates file 'conan_find_requirements.cmake' in the build directory,
    containing `find_package` statements for each declared Conan dependency.
    """
    from glob import glob
    from conan.tools.files import save

    print("\n======== Generating conan_find_requirements.cmake ========\n")

    # Find all the .cmake files in the generators dir.
    #
    cmake_config_files = glob(os.path.join(self.build_folder, 'generators', '**.cmake'))

    # Get the current project's requirements.
    #
    conan_requirements = list(self.requires.values())

    # Build a map from conan package names to cmake find names.
    #
    conan_to_cmake_requirements = {}

    for requirement in conan_requirements:
        print(requirement)
        if requirement.build:
            print("... skipping build=True requirement")
            continue

        package_name = str(requirement.ref).split('/')[0]
        candidates = set()
        for candidate in (file_name for file_name in cmake_config_files
                          if (package_name.lower() in file_name.lower() and
                              'config' in file_name.lower())):
            start = candidate.lower().index(package_name.lower())
            end = start + len(package_name)
            candidates.add(candidate[start:end])

        # Hopefully at this point there is exactly one candidate name!
        #
        assert len(candidates) == 1
        conan_to_cmake_requirements[package_name] = list(candidates)[0]

    deps_helper_file = os.path.join(self.build_folder, "conan_find_requirements.cmake")
    save(self, deps_helper_file,
         ''.join([f"find_package({cmake_package} CONFIG REQUIRED)\n"
                  for conan_ref, cmake_package in conan_to_cmake_requirements.items()]))

    print(f"\nSaved file {deps_helper_file}\n")


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def run_like_main(fn):
    try:
        print(fn())
    except RuntimeError as ex:
        print(str(ex), file=sys.stderr)
        sys.exit(1)
