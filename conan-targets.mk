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
#  - CONAN_2
#
# Output vars:
#
#  - BUILD_TYPE
#  - BUILD_DIR
#
#----- --- -- -  -  -   -

ifeq ($(BUILD_TYPE),)
export BUILD_TYPE := RelWithDebInfo
endif

BUILD_DIR := $(PROJECT_DIR)/build/$(BUILD_TYPE)

#----- --- -- -  -  -   -
# Force some requirements to build from source to workaround
# OS-specific bugs.
#
BUILD_FROM_SRC :=
ifeq ($(OS),Windows_NT)
else
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Darwin)
    #BUILD_FROM_SRC += --build=b2 --build=libbacktrace
  endif
endif
#----- --- -- -  -  -   -

CONAN_CONFIG_FLAGS := $(shell "$(SCRIPT_DIR)/conan-config-flags.sh")

$(info CONAN_CONFIG_FLAGS is $(CONAN_CONFIG_FLAGS))

ifeq ($(CONAN_2),1)
  CONAN_INSTALL    := conan install    $(CONAN_CONFIG_FLAGS) --build=missing
  CONAN_BUILD      := conan build      $(CONAN_CONFIG_FLAGS)
  CONAN_EXPORT_PKG := conan export-pkg $(CONAN_CONFIG_FLAGS)
  CONAN_CREATE     := conan create     $(CONAN_CONFIG_FLAGS)
else
  CONAN_INSTALL    := conan install    $(CONAN_CONFIG_FLAGS) --build=missing
  CONAN_BUILD      := conan build      -s build_type=$(BUILD_TYPE)
  CONAN_EXPORT_PKG := conan export-pkg $(CONAN_CONFIG_FLAGS)
  CONAN_CREATE     := conan create     $(CONAN_CONFIG_FLAGS)
endif

#----- --- -- -  -  -   -
export NO_COLOR=1
export CLICOLOR=0

#=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
# Targets
#+++++++++++-+-+--+----- --- -- -  -  -   -

#----- --- -- -  -  -   -
.PHONY: install
install:
	mkdir -p "$(BUILD_DIR)"
	(cd "$(BUILD_DIR)" && $(CONAN_INSTALL) $(OPTIONS) $(BUILD_FROM_SRC) "$(PROJECT_DIR)")

#----- --- -- -  -  -   -
.PHONY: build
build:
	mkdir -p "$(BUILD_DIR)"
	(cd "$(BUILD_DIR)" && $(CONAN_BUILD) "$(PROJECT_DIR)")
	"$(SCRIPT_DIR)/generate-vscode-config.sh"

#----- --- -- -  -  -   -
.PHONY: export-pkg
export-pkg:
	$(CONAN_EXPORT_PKG) "$(PROJECT_DIR)"

#----- --- -- -  -  -   -
.PHONY: create
create:
	$(CONAN_CREATE) "$(PROJECT_DIR)"

#----- --- -- -  -  -   -
.PHONY: clean-pkg
clean-pkg:
	conan remove -f "$(PROJECT_NAME)/$(shell script/get-version.sh)"

#----- --- -- -  -  -   -
.PHONY: clean
clean:
	rm -rf "$(BUILD_DIR)"
	rm -rf "$(PROJECT_DIR)/test_package/build"
