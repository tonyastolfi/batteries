##=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++
#
# Copyright 2023, Anthony Paul Astolfi
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

CONAN_VERSION := $(shell conan --version | sed -E 's,[Cc]onan version 2(\.[0-9]+)*,2,g' || echo '1')
$(info conan-targets.mk: Detected Conan Version==$(CONAN_VERSION))

ifeq ($(BUILD_TYPE),)
export BUILD_TYPE := RelWithDebInfo
endif

BUILD_DIR := $(PROJECT_DIR)/build/$(BUILD_TYPE)
ifeq ($(CONAN_VERSION),2)
  BUILD_BIN_DIR := $(BUILD_DIR)
  BUILD_LIB_DIR := $(BUILD_DIR)
else
  BUILD_BIN_DIR := $(BUILD_DIR)/bin
  BUILD_LIB_DIR := $(BUILD_DIR)/lib
endif

#----- --- -- -  -  -   -
# Force some requirements to build from source to workaround
# OS-specific bugs.
#
BUILD_FROM_SRC :=
ifeq ($(OS),Windows_NT)
else
  UNAME_S := $(shell uname -s)
endif
#----- --- -- -  -  -   -

#----- --- -- -  -  -   -
# By default, set BATT_BUILD_TESTS to 1.
#
ifeq ($(BATT_BUILD_TESTS),)
  CONAN_ENV := $(CONAN_ENV) BATT_BUILD_TESTS=1
endif
#----- --- -- -  -  -   -

CONAN_CONFIG_FLAGS := $(shell BUILD_TYPE=$(BUILD_TYPE) "$(SCRIPT_DIR)/conan-config-flags.sh")

$(info CONAN_CONFIG_FLAGS is $(CONAN_CONFIG_FLAGS))

ifeq ($(CONAN_VERSION),2)
  CONAN_INSTALL    := $(CONAN_ENV) conan install    $(CONAN_CONFIG_FLAGS) --build=missing $(OPTIONS)
  CONAN_BUILD      := $(CONAN_ENV) conan build      $(CONAN_CONFIG_FLAGS) $(OPTIONS)
  CONAN_EXPORT_PKG := $(CONAN_ENV) conan export-pkg $(CONAN_CONFIG_FLAGS) $(OPTIONS)
  CONAN_CREATE     := $(CONAN_ENV) conan create     $(CONAN_CONFIG_FLAGS) $(OPTIONS)
  CONAN_REMOVE     := $(CONAN_ENV) conan remove --confirm
else
  CONAN_INSTALL    := $(CONAN_ENV) conan install    $(CONAN_CONFIG_FLAGS) --build=missing
  CONAN_BUILD      := $(CONAN_ENV) conan build --install-folder "$(BUILD_DIR)"
  CONAN_EXPORT_PKG := $(CONAN_ENV) conan export-pkg $(CONAN_CONFIG_FLAGS)
  CONAN_CREATE     := $(CONAN_ENV) conan create     $(CONAN_CONFIG_FLAGS)
  CONAN_REMOVE     := $(CONAN_ENV) conan remove -f
endif

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
