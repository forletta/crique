PROJECT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(PROJECT_DIR)/build

MINUNIT_DIR := $(PROJECT_DIR)/external/minunit

.PHONY: test
test: $(BUILD_DIR)/test
	$(BUILD_DIR)/test

$(BUILD_DIR)/test: $(PROJECT_DIR)/test.c $(PROJECT_DIR)/crique.c $(BUILD_DIR)
	gcc -std=c11 -g -I$(PROJECT_DIR) -I$(MINUNIT_DIR) $(PROJECT_DIR)/crique.c $(PROJECT_DIR)/test.c -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
