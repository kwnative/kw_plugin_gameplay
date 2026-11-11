# SPDX-FileCopyrightText: 2026 N.Leiten
# SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
REPO_ROOT := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
KWMOD_SDK_ROOT ?= $(abspath $(REPO_ROOT)/../kw_modloader)
include $(KWMOD_SDK_ROOT)/mk/plugin.mk
.DEFAULT_GOAL := all
GAMEPLAY_SOURCES := src/config.c src/gameplay.c
GAMEPLAY_OBJECTS := $(call objects,$(GAMEPLAY_SOURCES))
ALL_OBJECTS := $(sort $(GAMEPLAY_OBJECTS))
BIN_TARGETS := $(BIN_DIR)/kw_gameplay.dll
CONFIG_INPUTS := $(wildcard $(REPO_ROOT)/config/*)
CONFIG_TARGETS := $(patsubst $(REPO_ROOT)/config/%,$(CONFIG_DIR)/%,$(CONFIG_INPUTS))
.PHONY: all config developer clean test-config test-preservation
all: $(BIN_DIR)/kw_gameplay.dll config
developer: $(BIN_TARGETS) config
test-config:
	@mkdir -p "$(OUT_DIR)/tests"
	$(CC) -std=c11 -Wall -Wextra -Werror -I"$(REPO_ROOT)/include" -I"$(KWMOD_SDK_ROOT)/include" \
	    "$(REPO_ROOT)/tests/test_config.c" "$(REPO_ROOT)/src/config.c" "$(KWMOD_SDK_ROOT)/src/config/config.c" \
	    -o "$(OUT_DIR)/tests/test_config"
	"$(OUT_DIR)/tests/test_config"
test-preservation:
	@mkdir -p "$(OUT_DIR)/tests"
	$(CC) -m32 -std=c11 -O2 -Wall -Wextra -Werror -Wno-attributes -fno-pie -no-pie \
	    -I"$(REPO_ROOT)/include" -I"$(KWMOD_SDK_ROOT)/include" \
	    "$(REPO_ROOT)/tests/test_preservation.c" -o "$(OUT_DIR)/tests/test_preservation"
	qemu-i386 "$(OUT_DIR)/tests/test_preservation"
config: $(CONFIG_TARGETS)
$(CONFIG_DIR)/%: $(REPO_ROOT)/config/%
	@mkdir -p "$(@D)"
	cp "$<" "$@"
ENGINE_ARTIFACTS := $(KWMOD_CONFIG_LIB) $(KWMOD_ENGINE_DLL)
include $(KWMOD_SDK_ROOT)/mk/sdk_dependencies.mk
$(ENGINE_ARTIFACTS): | $(SDK_ARTIFACT_STAMP)
	@test -f "$@"
$(BIN_DIR)/kw_gameplay.dll: $(GAMEPLAY_OBJECTS) $(KWMOD_CONFIG_LIB) $(KWMOD_ENGINE_DLL)
	@mkdir -p "$(@D)"
	$(WINDOWS_CC) $(DLL_FLAGS) $(GAMEPLAY_OBJECTS) $(KWMOD_CONFIG_LIB)  -o "$@"
clean:
	$(RM) $(BIN_TARGETS) $(ALL_OBJECTS) $(ALL_OBJECTS:.o=.d) $(CONFIG_TARGETS)
-include $(ALL_OBJECTS:.o=.d)

BUNDLE_FILES := $(BIN_DIR)/kw_gameplay.dll=plugins/kw_gameplay.dll \
    $(REPO_ROOT)/config/kw_gameplay.kwplugin=plugins/kw_gameplay.kwplugin \
    $(REPO_ROOT)/config/kw_gameplay.ini=plugins/kw_gameplay.ini
include $(KWMOD_SDK_ROOT)/mk/bundle.mk
