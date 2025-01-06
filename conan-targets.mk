##=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++
#
# Copyright 2023-2024, Anthony Paul Astolfi
#
#+++++++++++-+-+--+----- --- -- -  -  -   -

#----- --- -- -  -  -   -
# Required vars:
#
#  - PROJECT_DIR
#  - PROJECT_NAME
#  - SCRIPT_DIR
#
# Output vars:
#
#  - BUILD_TYPE
#  - BUILD_DIR
#
#----- --- -- -  -  -   -

ifeq ($(SCRIPT_DIR),)
  SCRIPT_DIR := $(TOOLS_DIR)
endif

CONAN_VERSION := $(shell conan --version | sed -E 's,[Cc]onan version 2(\.[0-9]+)*,2,g' || echo '1')
$(info conan-targets.mk: Detected Conan Version==$(CONAN_VERSION))

#----- --- -- -  -  -   -
ifeq ($(BUILD_TYPE),)
export BUILD_TYPE := RelWithDebInfo
endif

#----- --- -- -  -  -   -
export BUILD_DIR := $(PROJECT_DIR)/build/$(BUILD_TYPE)
ifeq ($(CONAN_VERSION),2)
  BUILD_BIN_DIR := $(BUILD_DIR)
  BUILD_LIB_DIR := $(BUILD_DIR)
else
  BUILD_BIN_DIR := $(BUILD_DIR)/bin
  BUILD_LIB_DIR := $(BUILD_DIR)/lib
endif

#----- --- -- -  -  -   -
CONAN_CONFIG_FLAGS := $(shell BUILD_TYPE=$(BUILD_TYPE) "$(SCRIPT_DIR)/conan-config-flags.sh")

$(info CONAN_CONFIG_FLAGS is $(CONAN_CONFIG_FLAGS))

#----- --- -- -  -  -   -
CONAN_HOME_DIR := $(shell $(CONAN_ENV) conan config home)
EXTERNAL_FILE_LOCK := $(dir $(CONAN_HOME_DIR))_batt_conan_lock

#----- --- -- -  -  -   -
ifeq ($(OS),Windows_NT)
  MUTEX :=
else
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Linux)
    MUTEX := flock --exclusive --timeout 500 "$(EXTERNAL_FILE_LOCK)"
  endif
  ifeq ($(UNAME_S),Darwin)
    MUTEX :=
  endif
endif
#----- --- -- -  -  -   -

#----- --- -- -  -  -   -
# By default, set BATT_BUILD_TESTS to 1.
#
ifeq ($(BATT_BUILD_TESTS),)
  CONAN_ENV := $(CONAN_ENV) BATT_BUILD_TESTS=1
endif
#----- --- -- -  -  -   -

CONAN_INSTALL_SH := $(SCRIPT_DIR)/conan-install.sh
CONAN_INSTALL    := $(CONAN_ENV) $(MUTEX) $(CONAN_INSTALL_SH) $(CONAN_CONFIG_FLAGS) --build=missing $(OPTIONS)
CONAN_BUILD      := $(CONAN_ENV) conan build      $(CONAN_CONFIG_FLAGS) $(OPTIONS) -c tools.build:skip_test=True
CONAN_EXPORT_PKG := $(CONAN_ENV) conan export-pkg $(CONAN_CONFIG_FLAGS) $(OPTIONS)
CONAN_CREATE     := $(CONAN_ENV) conan create     $(CONAN_CONFIG_FLAGS) $(OPTIONS)
CONAN_REMOVE     := $(CONAN_ENV) conan remove --confirm

#----- --- -- -  -  -   -
export NO_COLOR=1
export CLICOLOR=0

#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# Targets
#+++++++++++-+-+--+----- --- -- -  -  -   -

.PHONY: setup-conan
setup-conan:
	(test -f /setup-conan.sh && /setup-conan.sh || echo "Using ambient Conan config (/setup-conan.sh not found)")

#----- --- -- -  -  -   -
.PHONY: install
install: setup-conan
	mkdir -p "$(BUILD_DIR)"
	(cd "$(BUILD_DIR)" && $(CONAN_INSTALL) $(BUILD_FROM_SRC) "$(PROJECT_DIR)")

#----- --- -- -  -  -   -
.PHONY: build
build: setup-conan
	(cd "$(BUILD_DIR)" && $(CONAN_BUILD) "$(PROJECT_DIR)")
	"$(SCRIPT_DIR)/generate-vscode-config.sh"

#----- --- -- -  -  -   -
.PHONY: test
test:
	$(SCRIPT_DIR)/run-tests.sh

#----- --- -- -  -  -   -
.PHONY: code-coverage
code-coverage: $(BUILD_DIR)
	(gcovr --gcov-ignore-parse-errors && gcovr --gcov-ignore-parse-errors --cobertura -o $(BUILD_DIR)/code-coverage-results.xml)

#----- --- -- -  -  -   -
.PHONY: export-pkg
export-pkg: setup-conan
	$(CONAN_EXPORT_PKG) "$(PROJECT_DIR)"

#----- --- -- -  -  -   -
.PHONY: create
create: setup-conan
	$(CONAN_CREATE) "$(PROJECT_DIR)"

#----- --- -- -  -  -   -
.PHONY: clean-pkg
clean-pkg:
	$(CONAN_REMOVE) "$(PROJECT_NAME)/$(shell script/get-version.sh)"

#----- --- -- -  -  -   -
.PHONY: clean
clean:
	rm -rf "$(BUILD_DIR)"
	rm -rf "$(PROJECT_DIR)/test_package/build"

#----- --- -- -  -  -   -
.PHONY: cmake-build
cmake-build:
	cmake --build "$(BUILD_DIR)" -- -j$(shell nproc)
