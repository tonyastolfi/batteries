##=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++
# Copyright 2022 Anthony Paul Astolfi
#
from conans import ConanFile, CMake

import os, sys

SCRIPT_DIR = os.path.join(os.path.dirname(__file__), 'script')
VERBOSE = os.getenv('VERBOSE') and True or False

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
# Import the Batteries utility module.
#
sys.path.append(SCRIPT_DIR)

import batt

batt.VERBOSE = False
VERSION = batt.get_version(no_check_conan=True)
batt.verbose(f'VERSION={VERSION}')
#
#+++++++++++-+-+--+----- --- -- -  -  -   -

class BatteriesConan(ConanFile):
    name = "batteries"
    version = VERSION
    license = "Apache Public License 2.0"
    author = "Tony Astolfi <tastolfi@gmail.com>"
    url = "https://github.com/tonyastolfi/batteries.git"
    description = "C++ Essentials left out of std:: and boost::"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = {"shared": False}
    generators = "cmake"
    build_policy = "missing"
    requires = [
        "gtest/1.11.0",
        "boost/1.79.0",
    ]
    exports_sources = [
        "src/CMakeLists.txt",
        "src/batteries.hpp",
        "src/batteries/*.hpp",
        "src/batteries/*/*.hpp",
        "src/batteries/*_test.cpp",
        "src/batteries/*.test.cpp",
        "src/batteries/**/*.hpp",
        "src/batteries/**/*_test.cpp",
        "src/batteries/**/*.test.cpp",
        "src/batteries/*.ipp",
        "src/batteries/*/*.ipp",
        "src/batteries/**/*.ipp",
        "script/*.sh",
    ]

    def configure(self):
        self.options["gtest"].shared = False
        self.options["boost"].shared = False

    def build(self):
        cmake = CMake(self)
        cmake.verbose = VERBOSE
        cmake.configure(source_folder="src")
        cmake.build()

    def export(self):
        self.copy("*.sh", src="script", dst="script")

    def package(self):
        self.copy("*.hpp", dst="include", src="src")
        self.copy("*.ipp", dst="include", src="src")
        self.copy("*.sh", dst="script", src="script")

    def package_info(self):
        self.cpp_info.cxxflags = ["-D_GNU_SOURCE"]
        self.cpp_info.system_libs = ["dl"]

    def package_id(self):
        self.info.header_only()
