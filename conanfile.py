from conans import ConanFile, CMake

import os

SCRIPT_DIR_ = os.path.join(os.path.dirname(__file__), 'script')
print(f"SCRIPT_DIR_={SCRIPT_DIR_}")

VERSION_ = os.popen(os.path.join(SCRIPT_DIR_, "get-version.sh")).read().strip()
print(f"VERSION_={VERSION_}")

class BatteriesConan(ConanFile):
    name = "batteries"
    version = VERSION_
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
        "gtest/cci.20210126",
        "boost/1.77.0",
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
    ]
    default_user = "tastolfi+batteries"
    default_channel = "stable"

    def configure(self):
        self.options["gtest"].shared = False
        self.options["boost"].shared = False

    def build(self):
        cmake = CMake(self)
        cmake.verbose = os.getenv('VERBOSE') and True or False
        cmake.definitions["BUILD_DOC"] = "ON"
        cmake.configure(source_folder="src")
        cmake.build()
        #cmake.test(output_on_failure=True)

        # Explicit way:
        # self.run('cmake %s/hello %s'
        #          % (self.source_folder, cmake.command_line))
        # self.run("cmake --build . %s" % cmake.build_config)

    def package(self):
        self.copy("*.hpp", dst="include", src="src")
        self.copy("*.ipp", dst="include", src="src")

    def package_info(self):
        self.cpp_info.cxxflags = ["-std=c++17 -D_GNU_SOURCE"]
        self.cpp_info.system_libs = ["dl"]

    def package_id(self):
        self.info.header_only()
