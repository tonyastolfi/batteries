.PHONY: build build-nodoc install create test publish docker-build docker-push docker

ifeq ($(BUILD_TYPE),)
BUILD_TYPE := RelWithDebInfo
endif

PROJECT_DIR := $(shell pwd)
BUILD_DIR := build/$(BUILD_TYPE)
DOC_DIR := $(BUILD_DIR)/doc

build: | install
	mkdir -p "$(BUILD_DIR)"
	(cd "$(BUILD_DIR)" && conan build ../..)

test: build
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
	(cd "$(BUILD_DIR)" && conan install ../.. -s build_type=$(BUILD_TYPE) --build=missing)

create: test
	(cd "$(BUILD_DIR)" && conan create ../.. -s build_type=$(BUILD_TYPE))


publish: | test build
	script/publish-release.sh


docker-build:
	(cd docker && docker build -t registry.gitlab.com/tonyastolfi/batteries .)

docker-push: | docker-build
	(cd docker && docker push registry.gitlab.com/tonyastolfi/batteries)


docker: docker-build docker-push


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
