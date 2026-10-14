PROJECT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(PROJECT_DIR)/build

MINUNIT_DIR := $(PROJECT_DIR)/external/minunit

.PHONY: test
test: $(BUILD_DIR)/crique
	$(BUILD_DIR)/crique

$(BUILD_DIR)/crique: $(PROJECT_DIR)/crique.c $(PROJECT_DIR)/crique.h $(BUILD_DIR)
	gcc -std=c11 -g -DCRIQUE_DEBUG_ASSERT -DCRIQUE_TEST -I$(PROJECT_DIR) -I$(MINUNIT_DIR) $(PROJECT_DIR)/crique.c -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
