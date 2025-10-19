BUILD_DIR ?= build
CMAKE ?= cmake
CTEST ?= ctest
PYTHON ?= python3
CLANG_FORMAT ?= clang-format
CLANG_TIDY ?= clang-tidy

.PHONY: all help configure build test examples format tidy clean

all: build

help:
	@echo "Available targets:"
	@echo "  configure  - run CMake configure step (with tests enabled)"
	@echo "  build      - build the project via CMake"
	@echo "  test       - run ctest with output on failure"
	@echo "  examples   - build all example binaries"
	@echo "  format     - run clang-format on C/H sources"
	@echo "  tidy       - run clang-tidy using compile_commands.json"
	@echo "  clean      - clean CMake build directory"

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DBUILD_TESTING=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

build: configure
	$(CMAKE) --build $(BUILD_DIR)

test: build
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

examples: build
	$(CMAKE) --build $(BUILD_DIR) --target modbus_tcp_server modbus_tcp_client modbus_rtu_server modbus_rtu_client example_tcp_client_fc03

format:
	$(PYTHON) tools/maint/format_all.py --clang-format "$(CLANG_FORMAT)"

tidy: configure
	$(PYTHON) tools/maint/tidy_all.py --clang-tidy "$(CLANG_TIDY)" --build-dir "$(BUILD_DIR)"

clean:
	@if [ -d "$(BUILD_DIR)" ]; then \
		$(CMAKE) --build $(BUILD_DIR) --target clean; \
	fi
