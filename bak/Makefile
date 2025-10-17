.DEFAULT_GOAL := help

HOST_PRESET := host-debug
HOST_BUILD_DIR := build/$(HOST_PRESET)
CLANG_TIDY_BUILD_DIR := build/clang-tidy
EXAMPLES_PRESET := host-debug-examples
EXAMPLES_BUILD_DIR := build/$(EXAMPLES_PRESET)
RUN_CLANG_TIDY ?= run-clang-tidy
CLANG_TIDY_ARGS ?= -quiet
CLANG_TIDY_FILES ?=
CLANG_TIDY_JOBS ?=
CLANG_TIDY_SDKROOT ?= $(shell xcrun --show-sdk-path 2>/dev/null)
CLANG_TIDY_ENV ?= $(if $(strip $(CLANG_TIDY_SDKROOT)),SDKROOT=$(CLANG_TIDY_SDKROOT))

.PHONY: help configure build test examples lint tidy-config footprint footprint-check docs clean

help:
	@echo "Available targets:"
	@echo "  make build        - Configure (if needed) and build the $(HOST_PRESET) preset"
	@echo "  make test         - Build and run the host test suite with ctest"
	@echo "  make examples     - Build every example via the $(EXAMPLES_PRESET) preset"
	@echo "  make lint         - Run clang-tidy analysis using build/clang-tidy"
	@echo "  make footprint    - Rebuild ROM/RAM footprint snapshots for host/stm32g0/esp32c3"
	@echo "  make footprint-check - Check footprint against baseline (CI mode)"
	@echo "  make docs         - Generate HTML documentation via the docs preset"
	@echo "  make clean        - Remove build trees (host/clang-tidy/examples/docs) and footprint artifacts"

configure:
	cmake --preset $(HOST_PRESET)

build: configure
	cmake --build --preset $(HOST_PRESET)

examples:
	cmake --build --preset $(EXAMPLES_PRESET)

# Run the full host test suite with verbose failures
# Requires Ninja and the host-debug preset configured.
test: build
	ctest --test-dir $(HOST_BUILD_DIR) --output-on-failure

# Configure the clang-tidy build tree.
tidy-config:
	cmake -S . -B $(CLANG_TIDY_BUILD_DIR) -G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DMODBUS_ENABLE_TESTS=OFF \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DCMAKE_C_CLANG_TIDY= \
		-DCMAKE_CXX_CLANG_TIDY=

# Run clang-tidy against the code base. The first invocation will configure automatically.
lint:
	@if [ ! -f $(CLANG_TIDY_BUILD_DIR)/CMakeCache.txt ]; then \
		$(MAKE) tidy-config; \
	fi
	cmake --build $(CLANG_TIDY_BUILD_DIR) --parallel
	$(CLANG_TIDY_ENV) $(RUN_CLANG_TIDY) -p $(CLANG_TIDY_BUILD_DIR) $(CLANG_TIDY_ARGS) \
		$(if $(strip $(CLANG_TIDY_JOBS)),-j $(CLANG_TIDY_JOBS),) $(CLANG_TIDY_FILES)

footprint:
	rm -rf build/footprint
	python3 scripts/ci/report_footprint.py \
		--profiles TINY LEAN FULL \
		--targets host stm32g0 esp32c3 \
		--generator Ninja \
		--output build/footprint/metrics.json \
		--update-readme README.md

# Gate 30: Check footprint against baseline (CI mode)
footprint-check:
	@echo "📊 Checking footprint against baseline..."
	python3 scripts/ci/report_footprint.py \
		--profiles TINY LEAN FULL \
		--targets host \
		--baseline scripts/ci/footprint_baseline.json \
		--threshold 0.05 \
		--output build/footprint/metrics.json

docs:
	cmake --build --preset docs

clean:
	rm -rf $(HOST_BUILD_DIR) $(CLANG_TIDY_BUILD_DIR) $(EXAMPLES_BUILD_DIR) build/host-docs build/footprint
