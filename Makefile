.PHONY: build build-nodoc install create test publish docker-build docker-push docker

#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

ifeq ($(BUILD_TYPE),)
BUILD_TYPE := RelWithDebInfo
endif

PROJECT_DIR := $(shell pwd)
BUILD_DIR := build/$(BUILD_TYPE)
DOC_DIR := $(BUILD_DIR)/doc

# The list of OS/Compiler/Arch variants to build images for.
#
DOCKER_PLATFORM_VARIANTS := \
	linux_gcc9_amd64 \
	linux_gcc11_amd64 \


DOCKER_IMAGE_PREFIX := registry.gitlab.com/batteriescpp/batteries
DOCKER_TAG_VERSION_PREFIX := v$(shell "$(PROJECT_DIR)/script/get-version.sh")
DOCKER_TAGS_TO_BUILD := $(foreach VARIANT,$(DOCKER_PLATFORM_VARIANTS),$(DOCKER_IMAGE_PREFIX):$(DOCKER_TAG_VERSION_PREFIX)$(VARIANT))
DOCKER_TS_DIR := $(BUILD_DIR)/docker
DOCKER_TS_BUILD_LIST := $(foreach VARIANT,$(DOCKER_PLATFORM_VARIANTS),$(DOCKER_TS_DIR)/$(VARIANT).build.ts)
DOCKER_TS_PUSH_LIST := $(foreach VARIANT,$(DOCKER_PLATFORM_VARIANTS),$(DOCKER_TS_DIR)/$(VARIANT).push.ts)

CONAN_PROFILE := $(shell test -f /etc/conan_profile.default && echo '/etc/conan_profile.default' || echo 'default')

$(info CONAN_PROFILE is $(CONAN_PROFILE))
$(info DOCKER_TAGS_TO_BUILD is $(DOCKER_TAGS_TO_BUILD))
$(info DOCKER_TS_BUILD_LIST is $(DOCKER_TS_BUILD_LIST))
$(info DOCKER_TS_PUSH_LIST is $(DOCKER_TS_PUSH_LIST))
#==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

build: | install
	mkdir -p "$(BUILD_DIR)"
	(cd "$(BUILD_DIR)" && conan build ../..)

test: | build
	mkdir -p "$(BUILD_DIR)"
ifeq ("$(GTEST_FILTER)","")
	@echo -e "\n\nRunning DEATH tests ==============================================\n"
	(cd "$(BUILD_DIR)" && GTEST_OUTPUT='xml:../death-test-results.xml' GTEST_FILTER='*Death*' ctest --verbose)
	@echo -e "\n\nRunning non-DEATH tests ==========================================\n"
	(cd "$(BUILD_DIR)" && GTEST_OUTPUT='xml:../test-results.xml' GTEST_FILTER='*-*Death*' ctest --verbose)
else
	(cd "$(BUILD_DIR)" && GTEST_OUTPUT='xml:../test-results.xml' ctest --verbose)
endif

install:
	mkdir -p "$(BUILD_DIR)"
	(cd "$(BUILD_DIR)" && conan install --profile "$(CONAN_PROFILE)" -s build_type=$(BUILD_TYPE) --build=missing ../..)

create: test
	(cd "$(BUILD_DIR)" && conan create --profile "$(CONAN_PROFILE)" -s build_type=$(BUILD_TYPE) ../..)


publish: | test build
	script/publish-release.sh


#+++++++++++-+-+--+----- --- -- -  -  -   -
#
$(DOCKER_TS_DIR)/%.build.ts: $(PROJECT_DIR)/docker/Dockerfile.% $(MAKEFILE_LIST)
	mkdir -p "$(DOCKER_TS_DIR)"
	(cd "$(PROJECT_DIR)/docker" && docker build -t $(DOCKER_IMAGE_PREFIX):$(DOCKER_TAG_VERSION_PREFIX).$* -f Dockerfile.$* .)
	docker tag $(DOCKER_IMAGE_PREFIX):$(DOCKER_TAG_VERSION_PREFIX).$* $(DOCKER_IMAGE_PREFIX):latest.$*
	docker tag $(DOCKER_IMAGE_PREFIX):$(DOCKER_TAG_VERSION_PREFIX).$* $(DOCKER_IMAGE_PREFIX):latest
	touch "$@"


docker-build: $(DOCKER_TS_BUILD_LIST)

#+++++++++++-+-+--+----- --- -- -  -  -   -
#
$(DOCKER_TS_DIR)/%.push.ts: $(DOCKER_TS_DIR)/%.build.ts $(MAKEFILE_LIST)
	docker push $(DOCKER_IMAGE_PREFIX):$(DOCKER_TAG_VERSION_PREFIX).$*
	docker push $(DOCKER_IMAGE_PREFIX):latest.$*
	docker push $(DOCKER_IMAGE_PREFIX):latest
	touch "$@"


docker-push: $(DOCKER_TS_PUSH_LIST)


docker: docker-build docker-push


#+++++++++++-+-+--+----- --- -- -  -  -   -
#
.PHONY: doxygen
doxygen:
	rm -rf "$(PROJECT_DIR)/build/doxygen"
	mkdir -p "$(PROJECT_DIR)/build/doxygen"
	doxygen "$(PROJECT_DIR)/config/doxygen.config" >"$(PROJECT_DIR)/build/doxybook2.log" 2>&1


.PHONY: doxybook2
doxybook2: doxygen
	rm -rf "$(PROJECT_DIR)/build/doxybook2"
	mkdir -p "$(PROJECT_DIR)/build/doxybook2/output"
	doxybook2 --config "$(PROJECT_DIR)/config/doxybook_config.json" --input "$(PROJECT_DIR)/build/doxygen/xml" --output "$(PROJECT_DIR)/build/doxybook2/output" --templates "$(PROJECT_DIR)/config/doxybook_templates" --debug-templates >"$(PROJECT_DIR)/build/doxybook2.log" 2>&1 || cat "$(PROJECT_DIR)/build/doxybook2.log"


.PHONY: mkdocs
mkdocs: doxybook2
	mkdir -p "$(PROJECT_DIR)/build/mkdocs"
	cp -aT "$(PROJECT_DIR)/doc" "$(PROJECT_DIR)/build/mkdocs/docs"
	cp -aT "$(PROJECT_DIR)/config/mkdocs_overrides" "$(PROJECT_DIR)/build/mkdocs/overrides"
	cp -a "$(PROJECT_DIR)/config/mkdocs.yml" "$(PROJECT_DIR)/build/mkdocs/"
	rm -rf "$(PROJECT_DIR)/build/mkdocs/docs/_autogen"
	cp -aT "$(PROJECT_DIR)/build/doxybook2/output" "$(PROJECT_DIR)/build/mkdocs/docs/_autogen"


.PHONY: clean-docs
clean-docs:
	rm -rf "$(PROJECT_DIR)/build/mkdocs"
	rm -rf "$(PROJECT_DIR)/build/doxybook2"
	rm -rf "$(PROJECT_DIR)/build/doxygen"

.PHONY: serve-docs
serve-docs: mkdocs
	cd "$(PROJECT_DIR)/build/mkdocs" && mkdocs serve

.PHONY: deploy-docs
deploy-docs: mkdocs
	cd "$(PROJECT_DIR)/build/mkdocs" && mkdocs build
	"$(PROJECT_DIR)/script/deploy-docs.sh" "$(PROJECT_DIR)/build/mkdocs/site" "$(PROJECT_DIR)/build/mkdocs-deploy"
