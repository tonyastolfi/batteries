.PHONY: build build-nodoc install create

ifeq ($(BUILD_TYPE),)
BUILD_TYPE := RelWithDebInfo
endif

build:
	mkdir -p build/$(BUILD_TYPE)
	(cd build/$(BUILD_TYPE) && conan build ../..)

test: build
	mkdir -p build/$(BUILD_TYPE)
	(cd build/$(BUILD_TYPE) && ctest --verbose)

install:
	mkdir -p build/$(BUILD_TYPE)
	(cd build/$(BUILD_TYPE) && conan install ../.. -s build_type=$(BUILD_TYPE) --build=missing)

create:
	(cd build/$(BUILD_TYPE) && conan create ../..)
