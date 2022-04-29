.PHONY: build build-nodoc install create test publish docker-build docker-push docker

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
	(cd build/$(BUILD_TYPE) && conan create ../.. -s build_type=$(BUILD_TYPE))


publish: | test build
	script/publish-release.sh


docker-build:
	(cd docker && docker build -t registry.gitlab.com/tonyastolfi/batteries .)


docker-push: | docker-build
	(cd docker && docker push registry.gitlab.com/tonyastolfi/batteries)


docker: docker-build docker-push
