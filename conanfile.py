from conans import ConanFile, CMake


class BatteriesConan(ConanFile):
    name = "batteries"
    version = "0.1.0"
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
        "gtest/1.8.0@conan/stable",
        "boost/1.71.0@conan/stable",
    ]
    exports_sources = [
        "src/CMakeLists.txt",
        "src/batteries.hpp",
        "src/batteries/*.hpp",
        "src/batteries/*_test.cpp",
    ]

    def configure(self):
        self.options["gtest"].shared = False
        self.options["boost"].shared = False

    def build(self):
        cmake = CMake(self)
        cmake.configure(source_folder="src")
        cmake.build()
        cmake.test(output_on_failure=True)

        # Explicit way:
        # self.run('cmake %s/hello %s'
        #          % (self.source_folder, cmake.command_line))
        # self.run("cmake --build . %s" % cmake.build_config)

    def package(self):
        self.copy("*.hpp", dst="include", src="src")

    def package_info(self):
        self.cpp_info.cxxflags = ["-std=c++17 -D_GNU_SOURCE"]
        self.cpp_info.system_libs = ["dl"]

    def package_id(self):
        self.info.header_only()
