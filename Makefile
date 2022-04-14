.PHONY: build build-nodoc install create

ifeq ($(BUILD_TYPE),)
BUILD_TYPE := RelWithDebInfo
endif

build: | install
	mkdir -p build/$(BUILD_TYPE)
	(cd build/$(BUILD_TYPE) && conan build ../..)

test: build
	mkdir -p build/$(BUILD_TYPE)
	(cd build/$(BUILD_TYPE) && ctest --verbose)

install:
	mkdir -p build/$(BUILD_TYPE)
	(cd build/$(BUILD_TYPE) && conan install ../.. -s build_type=$(BUILD_TYPE) --build=missing)

create: test
	(cd build/$(BUILD_TYPE) && conan create ../..)


publish: | test build
	script/publish-release.sh
