#
# Copyright 2022-2023 Anthony Paul Astolfi
#
import os, sys, re, platform


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
def is_header_only_project(project):
    """
    Detects whether the package is header-only by checking (in this order):
      - self.options.header_only (bool)
      - self.info.options.header_only (bool)
      - self._is_header_only (bool)

    Returns True iff any of these checks indicates `self` is a header-only
    package.
    """
    options_header_only = False
    try:
        options_header_only = (
            hasattr(project, 'options') and
            hasattr(project.options, 'header_only') and
            project.options.header_only
        )
    except:
        pass

    info_options_header_only = False
    try:
        info_options_header_only = (
            hasattr(project, 'info') and
            hasattr(project.info, 'options') and
            hasattr(project.info.options, 'header_only') and
            project.info.options.header_only
        )
    except:
        pass

    return options_header_only or info_options_header_only or (
        hasattr(project, '_is_header_only') and
        project._is_header_only
    )


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# OVERRIDE, VISIBLE - kwargs bundle dictionaries to be used with
#  self.requires(ref, **OVERRIDE, ...), etc.
#
OVERRIDE={
    "override": True,
}
VISIBLE={
    "visible": True,
    "transitive_headers": True,
    "transitive_libs": True,
}


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def default_init_cmake(self):
    """
    Initializes, configures, and returns a CMake object.
    """
    from conan.tools.cmake import CMake
    cmake = CMake(self)
    cmake.verbose = VERBOSE

    cmake.configure()
    return cmake


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def set_version_from_git_tags(self):
    """
    Mix-in implementation of ConanFile.set_version.

    Uses GIT tags (release-MAJOR.MINOR.PATCH) to derive the version.
    """
    self.version = get_version(no_check_conan=True)
    verbose(f'VERSION={self.version}')


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def default_config_options(self):
    if self.settings.os == "Windows":
        del self.options.fPIC


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def default_imports(self):
    from conan.tools.files import copy
    copy(self, "*.dll", dst="bin", src="bin") # From bin to bin
    copy(self, "*.dylib*", dst="bin", src="lib") # From lib to bin
    default_config_options(self)


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def default_cmake_layout(self):
    """
    Mix-in implementation of ConanFile.layout.

    Uses conan.tools.cmake.cmake_layout, with default args
    (CMakeLists.txt directly in project dir).
    """
    from conan.tools.cmake import cmake_layout
    cmake_layout(self)


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def cmake_in_src_layout(self):
    """
    Mix-in implementation of ConanFile.layout.

    Uses conan.tools.cmake.cmake_layout, with src_folder="src".
    """
    from conan.tools.cmake import cmake_layout
    cmake_layout(self, src_folder="src")


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def default_cmake_generate(self):
    """
    Mix-in implementation of ConanFile.generate.

    Generates a file named <project_name>_options.cmake in the build directory
    containing CMake variables for each declared option in the package.  The
    variable names are constructed using the pattern:

      <PROJECT_NAME>_OPTION_<OPTION_NAME>

    If the ConanFile object has a _get_cxx_flags() method, then this method is
    invoked to obtain a list of C++ compiler flags, which are also written to
    the <project_name>_options.cmake file.

    Uses CMakeToolchain and CMakeDeps to configure the project for build.
    """
    from conan.tools.files import save
    from conan.tools.cmake import CMakeToolchain, CMakeDeps

    # Write the current options to a .cmake file
    #
    opts_file = os.path.join(self.build_folder, f"{self.name}_options.cmake")

    if hasattr(self, "_get_cxx_flags"):
        save(self, opts_file,
             '\n'.join([f"add_definitions(\"{flag}\")"
                        for flag in self._get_cxx_flags()]))

    # Configure CMake toolchain.
    #
    tc = CMakeToolchain(self, generator='Ninja')

    for option_name, _ in self.options.possible_values.items():
        var_name = f"{self.name.upper()}_OPTION_{option_name.upper()}"
        tc.variables[var_name] = getattr(self.options, option_name)

    tc.generate()

    # Generate CMake files for required packages.
    #
    deps = CMakeDeps(self)
    deps.generate()

    generate_conan_find_requirements(self)


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def default_cmake_build(self):
    """
    Mix-in implementation of ConanFile.build.

    Uses CMake to configure and build the package.
    """
    cmake = default_init_cmake(self)
    cmake.build()


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def default_conan_lib_package(self):
    """
    Mix-in implementation of ConanFile.package.

    Copies license files, header files, and all known library types
    (.a, .so, .lib, .dll, and .dylib).
    """
    from conan.tools.files import copy

    src_build = self.build_folder
    src_include = os.path.join(self.source_folder, ".")
    dst_licenses = os.path.join(self.package_folder, "licenses")
    dst_include = os.path.join(self.package_folder, "include")
    dst_lib = os.path.join(self.package_folder, "lib")
    dst_bin = os.path.join(self.package_folder, "bin")

    copy(self, "LICENSE", src=self.source_folder, dst=dst_licenses)
    #----- --- -- -  -  -   -
    copy(self, pattern="*.hpp", src=src_include, dst=dst_include)
    copy(self, pattern="*.ipp", src=src_include, dst=dst_include)
    #----- --- -- -  -  -   -
    copy(self, pattern="*.a", src=src_build, dst=dst_lib, keep_path=False)
    copy(self, pattern="*.so", src=src_build, dst=dst_lib, keep_path=False)
    copy(self, pattern="*.lib", src=src_build, dst=dst_lib, keep_path=False)
    #----- --- -- -  -  -   -
    copy(self, pattern="*.dll", src=src_build, dst=dst_bin, keep_path=False)
    copy(self, pattern="*.dylib", src=src_build, dst=dst_lib, keep_path=False)


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def default_cmake_lib_package(self):
    """
    Mix-in implementation of ConanFile.package.

    Copies license files, header files, and all known library types
    (.a, .so, .lib, .dll, and .dylib).  Also does cmake install.
    """
    from conan.tools.cmake import CMake

    default_conan_lib_package(self)

    cmake = default_init_cmake(self)
    cmake.install()


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def default_lib_package_info(self):
    """
    Mix-in implementation of ConanFile.package_info.

    Configures the package to include system library 'libdl.so' on Linux.

    If the passed ConanFile object (self) has a `_get_cxx_flags()` method,
    then that method is invoked to obtain a list of C++ compiler flag args
    to set when linking against the package.

    Detects whether the package is header-only by checking (in this order):
      - self.options.header_only (bool)
      - self.info.options.header_only (bool)
      - self._is_header_only (bool)

    Iff the package _is not_ header-only, configures link options for the
    exported library (of the same name as the project: self.name).
    """
    if hasattr(self, "_get_cxx_flags"):
        self.cpp_info.cxxflags = list(self._get_cxx_flags())
    else:
        self.cpp_info.cxxflags = ["-std=c++17", "-D_GNU_SOURCE", "-D_BITS_UIO_EXT_H=1"]

    self.cpp_info.names["pkg_config"] = self.name

    if platform.system() == 'Linux':
        self.cpp_info.system_libs = ["dl"]

    if not is_header_only_project(self):
        self.cpp_info.libs = [self.name]


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def default_lib_package_id(self):
    """
    Mix-in implementation of ConanFile.package_id.

    Configures the package_id (used for binary compatibility) according to
    whether or not the package is declared as header-only.

    Detects whether the package is header-only by checking (in this order):
      - self.options.header_only (bool)
      - self.info.options.header_only (bool)
      - self._is_header_only (bool)
    """
    if is_header_only_project(self):
        self.info.clear()


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def run_like_main(fn):
    try:
        print(fn())
    except RuntimeError as ex:
        print(str(ex), file=sys.stderr)
        sys.exit(1)


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

        if CONAN_VERSION_2 and requirement.build:
            print("... skipping build=True requirement")
            continue

        # (override==True implies direct==False, but does not set it explicitly)
        #
        if CONAN_VERSION_2 and (not requirement.direct or requirement.override):
            print("... skipping direct=False requirement")
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
        assert len(candidates) == 1, f"Ambiguous cmake name: {package_name}, candidates: {candidates}"
        conan_to_cmake_requirements[package_name] = list(candidates)[0]

    deps_helper_file = os.path.join(self.build_folder, "conan_find_requirements.cmake")
    save(self, deps_helper_file,
         ''.join([f"find_package({cmake_package} CONFIG REQUIRED)\n"
                  for conan_ref, cmake_package in conan_to_cmake_requirements.items()]))

    print(f"\nSaved file {deps_helper_file}\n")


#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
#
def conanfile_requirements(self, deps, override_deps=[], platform_deps={}):
    """
    Calls self.requires for each of the given dependencies.

    If a dependency name appears in _both_ of deps and override_deps, then
    it is included as though it were just in deps, but with the additional
    keyword arg `override=True`.
    """
    if platform.system() in platform_deps:
        deps += platform_deps[platform.system()]

    deps = set(deps)
    override_deps = set(override_deps)

    for dep_name in deps:
        self.requires(dep_name,
                      visible=True,
                      transitive_headers=True,
                      transitive_libs=True,
                      force=True,
                      override=(dep_name in override_deps))

    for override_name in override_deps:
        if override_name not in deps:
            self.requires(override_name,
                          override=True)
