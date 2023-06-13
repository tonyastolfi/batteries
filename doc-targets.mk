##=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++
#
# Copyright 2023, Anthony Paul Astolfi
#
#+++++++++++-+-+--+----- --- -- -  -  -   -

#----- --- -- -  -  -   -
# Required vars:
#
#  - PROJECT_DIR
#  - SCRIPT_DIR
#
#----- --- -- -  -  -   -

#+++++++++++-+-+--+----- --- -- -  -  -   -
# Doxygen
#
DOXYGEN_OUTPUT_DIR := $(PROJECT_DIR)/build/doxygen
DOXYGEN_LOG_FILE := $(PROJECT_DIR)/build/doxygen.log

#----- --- -- -  -  -   -
.PHONY: doxygen
doxygen:
	rm -rf "$(DOXYGEN_OUTPUT_DIR)"
	mkdir -p "$(DOXYGEN_OUTPUT_DIR)"
	doxygen "$(PROJECT_DIR)/config/doxygen.config" >"$(DOXYGEN_LOG_FILE)" 2>&1

#+++++++++++-+-+--+----- --- -- -  -  -   -
# Doxybook2
#
DOXYBOOK2_OUTPUT_DIR := $(PROJECT_DIR)/build/doxybook2
DOXYBOOK2_LOG_FILE := $(PROJECT_DIR)/build/doxybook2.log

#----- --- -- -  -  -   -
.PHONY: doxybook2
doxybook2: doxygen
	rm -rf "$(DOXYBOOK2_OUTPUT_DIR)"
	mkdir -p "$(DOXYBOOK2_OUTPUT_DIR)/output"
	doxybook2 --config "$(PROJECT_DIR)/config/doxybook_config.json" --input "$(DOXYGEN_OUTPUT_DIR)/xml" --output "$(DOXYBOOK2_OUTPUT_DIR)/output" --templates "$(PROJECT_DIR)/config/doxybook_templates" --debug-templates >"$(DOXYBOOK2_LOG_FILE)" 2>&1 || cat "$(DOXYBOOK2_LOG_FILE)"

#+++++++++++-+-+--+----- --- -- -  -  -   -
# Mkdocs
#
MKDOCS_OUTPUT_DIR := $(PROJECT_DIR)/build/mkdocs
MKDOCS_DEPLOY_DIR := $(PROJECT_DIR)/build/mkdocs-deploy

#----- --- -- -  -  -   -
.PHONY: mkdocs
mkdocs: doxybook2
	pip3 list -o
	mkdir -p "$(MKDOCS_OUTPUT_DIR)"
	cp -aT "$(PROJECT_DIR)/doc" "$(MKDOCS_OUTPUT_DIR)/docs"
	cp -aT "$(PROJECT_DIR)/config/mkdocs_overrides" "$(MKDOCS_OUTPUT_DIR)/overrides"
	cp -a "$(PROJECT_DIR)/config/mkdocs.yml" "$(MKDOCS_OUTPUT_DIR)/"
	rm -rf "$(MKDOCS_OUTPUT_DIR)/docs/_autogen"
	cp -aT "$(DOXYBOOK2_OUTPUT_DIR)/output" "$(MKDOCS_OUTPUT_DIR)/docs/_autogen"


#----- --- -- -  -  -   -
.PHONY: clean-docs
clean-docs:
	rm -rf "$(MKDOCS_OUTPUT_DIR)"
	rm -rf "$(DOXYBOOK2_OUTPUT_DIR)"
	rm -rf "$(DOXYGEN_OUTPUT_DIR)"

#----- --- -- -  -  -   -
.PHONY: serve-docs
serve-docs: mkdocs
	cd "$(MKDOCS_OUTPUT_DIR)" && mkdocs serve

#----- --- -- -  -  -   -
.PHONY: deploy-docs
deploy-docs: mkdocs
	cd "$(MKDOCS_OUTPUT_DIR)" && mkdocs build
	"$(SCRIPT_DIR)/deploy-docs.sh" "$(MKDOCS_OUTPUT_DIR)/site" "$(MKDOCS_DEPLOY_DIR)"
