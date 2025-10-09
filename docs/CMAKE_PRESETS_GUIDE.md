# CMake Presets Guide

This guide explains how to use CMake presets for easy configuration and cross-compilation of ModbusCore.

## Overview

CMake presets provide named configurations that eliminate the need to remember complex command-line arguments. This library includes presets for:
- **Host development** (native builds for testing)
- **Embedded targets** (STM32G0, ESP32-C3, nRF52)
- **Special builds** (sanitizers, coverage, fuzzing)

---

## Quick Start

### List Available Presets

```bash
cmake --list-presets
```

Output:
```
Available configure presets:

  "host-debug"          - Host Debug
  "host-release"        - Host Release
  "host-asan"           - Host ASan/UBSan
  "stm32g0-tiny"        - STM32G0 TINY Profile
  "esp32c3-lean"        - ESP32-C3 LEAN Profile
  "nrf52-full"          - nRF52 FULL Profile
  ...
```

### Configure with a Preset

```bash
cmake --preset <preset-name>
```

Example:
```bash
cmake --preset stm32g0-tiny
```

### Build

```bash
cmake --build build/<preset-name>
```

Example:
```bash
cmake --build build/stm32g0-tiny
```

---

## Host Presets

For native development and testing on your workstation.

### host-debug
**Purpose:** Development and debugging

```bash
cmake --preset host-debug
cmake --build build/host-debug
```

**Configuration:**
- Build type: `Debug`
- Optimization: `-O0 -g`
- All features enabled (Client, Server, RTU, TCP, ASCII)
- All function codes enabled
- Tests enabled
- Examples enabled

**Use cases:**
- Development
- Debugging with GDB/LLDB
- Unit testing
- Integration testing

### host-release
**Purpose:** Production release build

```bash
cmake --preset host-release
cmake --build build/host-release
```

**Configuration:**
- Build type: `Release`
- Optimization: `-O3`
- All features enabled
- Tests disabled
- Examples disabled

**Use cases:**
- Production builds
- Performance benchmarking
- Release packaging

### host-client-only
**Purpose:** Client-only build for footprint testing

```bash
cmake --preset host-client-only
cmake --build build/host-client-only
```

**Configuration:**
- Client enabled, Server disabled
- Useful for measuring client-only footprint
- All transports enabled

### host-server-only
**Purpose:** Server-only build for footprint testing

```bash
cmake --preset host-server-only
cmake --build build/host-server-only
```

**Configuration:**
- Server enabled, Client disabled
- Useful for measuring server-only footprint
- All transports enabled

### host-rtu-only
**Purpose:** RTU-only build for footprint testing

```bash
cmake --preset host-rtu-only
cmake --build build/host-rtu-only
```

**Configuration:**
- RTU transport only (TCP and ASCII disabled)
- Useful for measuring RTU-only footprint

### host-tcp-only
**Purpose:** TCP-only build for footprint testing

```bash
cmake --preset host-tcp-only
cmake --build build/host-tcp-only
```

**Configuration:**
- TCP transport only (RTU and ASCII disabled)
- Useful for measuring TCP-only footprint

### host-footprint
**Purpose:** Minimal footprint measurement

```bash
cmake --preset host-footprint
cmake --build build/host-footprint
```

**Configuration:**
- `MinSizeRel` build type
- Client-only, RTU-only
- Essential function codes (FC03, FC06, FC16)
- No diagnostics
- Generates map file for analysis

**Use cases:**
- Measuring minimum footprint
- Verifying size optimizations
- Resource planning

### host-asan
**Purpose:** Address Sanitizer + Undefined Behavior Sanitizer

```bash
cmake --preset host-asan
cmake --build build/host-asan
./build/host-asan/tests/modbus_tests
```

**Configuration:**
- `-fsanitize=address,undefined`
- Detects memory leaks, buffer overflows, use-after-free
- Slower execution, higher memory usage

**Use cases:**
- Finding memory bugs
- Validating memory safety
- CI validation

### host-tsan
**Purpose:** Thread Sanitizer

```bash
cmake --preset host-tsan
cmake --build build/host-tsan
./build/host-tsan/tests/modbus_tests
```

**Configuration:**
- `-fsanitize=thread`
- Detects data races, deadlocks
- Slower execution

**Use cases:**
- Finding concurrency bugs
- Validating thread safety
- Multi-threaded testing

### host-coverage
**Purpose:** Code coverage analysis

```bash
cmake --preset host-coverage
cmake --build build/host-coverage
ctest --test-dir build/host-coverage
# Generate coverage report
cd build/host-coverage
make coverage  # or ninja coverage
```

**Configuration:**
- `--coverage` flags
- Generates `.gcda` and `.gcno` files
- lcov/gcov compatible

**Use cases:**
- Measuring test coverage
- Identifying untested code paths
- CI coverage reporting

### host-fuzz
**Purpose:** Fuzzing with libFuzzer or AFL

```bash
cmake --preset host-fuzz
cmake --build build/host-fuzz
# Run fuzzers (see build/host-fuzz/fuzz/)
```

**Configuration:**
- `-fsanitize=fuzzer` or AFL instrumentation
- Fuzzing harnesses built
- Corpus directories prepared

**Use cases:**
- Finding edge cases
- Robustness testing
- Security testing

---

## Embedded Target Presets

For cross-compilation to embedded ARM and RISC-V targets.

### Prerequisites

Install toolchains:

**ARM (STM32, nRF52):**
```bash
# macOS
brew install arm-none-eabi-gcc

# Ubuntu/Debian
sudo apt-get install gcc-arm-none-eabi

# Verify
arm-none-eabi-gcc --version
```

**RISC-V (ESP32-C3):**
```bash
# Option 1: ESP-IDF (recommended)
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh

# Option 2: Standalone toolchain
# Download from https://github.com/espressif/crosstool-NG/releases
```

**Ninja (recommended):**
```bash
# macOS
brew install ninja

# Ubuntu/Debian
sudo apt-get install ninja-build
```

---

### STM32G0 Presets

#### stm32g0-tiny
**Target:** STM32G0 series (Cortex-M0+)  
**Profile:** TINY (~26KB ROM, 8KB RAM)

```bash
cmake --preset stm32g0-tiny
cmake --build build/stm32g0-tiny
```

**Configuration:**
- CPU: Cortex-M0+ (`-mcpu=cortex-m0plus -mthumb`)
- Build type: `MinSizeRel` (`-Os`)
- Client-only, RTU-only
- Essential function codes (FC03, FC06, FC16)
- Queue depth: 8 items
- Tests and examples: Disabled

**Output files:**
- `build/stm32g0-tiny/modbus/libmodbus.a`
- `build/stm32g0-tiny/modbus.map` (memory map)

**Use cases:**
- Battery-powered sensors
- Low-cost MCUs (64-128KB flash)
- Space-constrained applications

#### stm32g0-lean
**Target:** STM32G0 series (Cortex-M0+)  
**Profile:** LEAN (~45KB ROM, 16KB RAM)

```bash
cmake --preset stm32g0-lean
cmake --build build/stm32g0-lean
```

**Configuration:**
- CPU: Cortex-M0+
- Build type: `MinSizeRel`
- Client + Server, RTU transport
- Common function codes (FC01-FC06, FC15-FC16)
- Queue depth: 16 items

**Use cases:**
- RTU gateway devices
- Balanced configurations
- Mid-range STM32G0 (256KB+ flash)

---

### ESP32-C3 Presets

#### esp32c3-lean
**Target:** ESP32-C3 (RISC-V 32-bit IMC)  
**Profile:** LEAN (~45KB ROM, 16KB RAM)

```bash
cmake --preset esp32c3-lean
cmake --build build/esp32c3-lean
```

**Configuration:**
- CPU: RISC-V 32-bit (`-march=rv32imc -mabi=ilp32`)
- Build type: `Release` (`-O2`)
- Client + Server, RTU + TCP
- Common function codes
- Queue depth: 16 items

**Output files:**
- `build/esp32c3-lean/modbus/libmodbus.a`
- Link with your ESP-IDF project

**Use cases:**
- WiFi-to-RTU gateway
- IoT devices with connectivity
- Balanced ESP32-C3 applications

**Note:** For full ESP-IDF integration, use `idf.py build` with component mode (see KCONFIG_GUIDE.md).

#### esp32c3-full
**Target:** ESP32-C3 (RISC-V)  
**Profile:** FULL (~85KB ROM, 32KB RAM)

```bash
cmake --preset esp32c3-full
cmake --build build/esp32c3-full
```

**Configuration:**
- CPU: RISC-V 32-bit
- Build type: `Release`
- Client + Server, All transports (RTU, TCP, ASCII)
- All function codes (FC01-FC17)
- Queue depth: 32 items

**Use cases:**
- Full-featured gateway
- Multi-protocol bridge
- Feature-rich applications

---

### nRF52 Presets

#### nrf52-full
**Target:** nRF52 series (Cortex-M4F)  
**Profile:** FULL (~85KB ROM, 32KB RAM)

```bash
cmake --preset nrf52-full
cmake --build build/nrf52-full
```

**Configuration:**
- CPU: Cortex-M4F (`-mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16`)
- Build type: `Release` (`-O2`)
- Client + Server, RTU + TCP
- All function codes
- Queue depth: 32 items
- Diagnostics enabled (counters + trace buffer)

**Output files:**
- `build/nrf52-full/modbus/libmodbus.a`
- `build/nrf52-full/modbus.map`

**Use cases:**
- BLE-to-Modbus gateway
- Full-featured Nordic devices
- Development and debugging

#### nrf52-lean
**Target:** nRF52 series (Cortex-M4F)  
**Profile:** LEAN (~45KB ROM, 16KB RAM)

```bash
cmake --preset nrf52-lean
cmake --build build/nrf52-lean
```

**Configuration:**
- CPU: Cortex-M4F
- Build type: `MinSizeRel` (`-Os`)
- Client + Server, RTU-only
- Common function codes
- Queue depth: 16 items

**Use cases:**
- Balanced nRF52 configurations
- Battery-constrained BLE devices
- Cost-optimized designs

---

## Special Presets

### docs
**Purpose:** Build documentation only

```bash
cmake --preset docs
cmake --build build/docs
```

**Configuration:**
- Only Doxygen target built
- No library compilation
- Fast documentation rebuilds

**Output:**
- `build/docs/html/index.html`

---

## Creating Custom Presets

Edit `CMakePresets.json` to add your own preset:

```json
{
    "name": "my-custom-preset",
    "displayName": "My Custom Configuration",
    "description": "Custom build for my target",
    "generator": "Ninja",
    "binaryDir": "${sourceDir}/build/my-custom",
    "toolchainFile": "${sourceDir}/cmake/toolchains/my-toolchain.cmake",
    "cacheVariables": {
        "CMAKE_BUILD_TYPE": "MinSizeRel",
        "MODBUS_PROFILE": "LEAN",
        "MODBUS_ENABLE_CLIENT": "ON",
        "MODBUS_ENABLE_SERVER": "ON",
        "MODBUS_TRANSPORT_RTU": "ON",
        "MODBUS_TRANSPORT_TCP": "OFF",
        "MODBUS_ENABLE_TESTS": "OFF"
    }
}
```

### Inheriting from Base Presets

You can inherit settings from existing presets:

```json
{
    "name": "stm32f4-custom",
    "inherits": "stm32g0-lean",
    "toolchainFile": "${sourceDir}/cmake/toolchains/stm32f4.cmake",
    "cacheVariables": {
        "MODBUS_QUEUE_DEPTH": "24"
    }
}
```

---

## Analyzing Build Output

### Memory Map Files

For embedded presets, `.map` files are generated:

```bash
# View memory sections
grep -A 20 "Memory Configuration" build/stm32g0-tiny/modbus.map

# View symbol sizes
grep -A 50 "Archive member included" build/stm32g0-tiny/modbus.map | sort -k2 -n
```

### Size Analysis

```bash
# ARM targets
arm-none-eabi-size build/stm32g0-tiny/modbus/libmodbus.a

# RISC-V targets
riscv32-esp-elf-size build/esp32c3-lean/modbus/libmodbus.a
```

### Automated Footprint Report

Use the footprint script:

```bash
# For embedded preset
python scripts/ci/report_footprint.py --preset stm32g0-tiny

# Compare presets
python scripts/ci/report_footprint.py --preset stm32g0-tiny --preset stm32g0-lean
```

---

## Integration with IDEs

### Visual Studio Code

Add to `.vscode/settings.json`:

```json
{
    "cmake.configureOnOpen": false,
    "cmake.configureSettings": {
        "CMAKE_PRESET": "host-debug"
    }
}
```

Or use CMake Tools extension to select preset from UI.

### CLion

CLion automatically detects `CMakePresets.json`. Select preset from:
- Settings → Build, Execution, Deployment → CMake
- Or from the CMake tool window

### Command Line Workflow

```bash
# Configure
cmake --preset host-debug

# Build
cmake --build build/host-debug

# Test
ctest --test-dir build/host-debug

# Install (if needed)
cmake --install build/host-debug --prefix /usr/local
```

---

## Troubleshooting

### Preset Not Found

**Problem:** `cmake --preset xxx` fails with "Could not find preset".

**Solutions:**
1. List available presets: `cmake --list-presets`
2. Check `CMakePresets.json` syntax
3. Verify preset name (case-sensitive)

### Toolchain Not Found

**Problem:** Cross-compilation fails with "Could not find toolchain".

**Solutions:**
1. Install toolchain (see Prerequisites section)
2. Check toolchain path in `cmake/toolchains/<target>.cmake`
3. Set `CMAKE_C_COMPILER` manually:
   ```bash
   cmake --preset stm32g0-tiny -DCMAKE_C_COMPILER=/path/to/arm-none-eabi-gcc
   ```

### Build Errors

**Problem:** Build fails with undefined references or missing symbols.

**Solutions:**
1. Clean build: `rm -rf build/<preset-name>`
2. Reconfigure: `cmake --preset <preset-name>`
3. Check that all required source files are compiled
4. Verify toolchain flags in toolchain file

### Out of Memory (Embedded)

**Problem:** Linker reports out of RAM or ROM.

**Solutions:**
1. Switch to smaller profile (FULL → LEAN → TINY)
2. Disable unused function codes
3. Reduce queue depth
4. Disable diagnostics
5. Check linker script memory sizes

---

## Best Practices

1. **Use presets for consistency** - Avoid manual CMake configuration
2. **Test with host presets first** - Validate logic before cross-compiling
3. **Use sanitizers in CI** - Catch bugs early with host-asan/host-tsan
4. **Measure footprint regularly** - Use host-footprint and embedded presets
5. **Document custom presets** - Add comments in CMakePresets.json
6. **Version control presets** - Commit CMakePresets.json with your code

---

## Preset Reference Table

| Preset | Target | Profile | ROM | RAM | Use Case |
|--------|--------|---------|-----|-----|----------|
| host-debug | Native | FULL | N/A | N/A | Development |
| host-release | Native | FULL | N/A | N/A | Production |
| host-footprint | Native | TINY | ~26KB | ~8KB | Footprint measurement |
| stm32g0-tiny | Cortex-M0+ | TINY | ~26KB | ~8KB | Minimal MCU |
| stm32g0-lean | Cortex-M0+ | LEAN | ~45KB | ~16KB | Balanced MCU |
| esp32c3-lean | RISC-V | LEAN | ~45KB | ~16KB | WiFi gateway |
| esp32c3-full | RISC-V | FULL | ~85KB | ~32KB | Full gateway |
| nrf52-full | Cortex-M4F | FULL | ~85KB | ~32KB | BLE gateway |
| nrf52-lean | Cortex-M4F | LEAN | ~45KB | ~16KB | BLE sensor |

---

## See Also

- [KCONFIG_GUIDE.md](KCONFIG_GUIDE.md) - Kconfig/menuconfig usage
- [FOOTPRINT.md](FOOTPRINT.md) - Memory usage details
- [RESOURCE_PLANNING.md](RESOURCE_PLANNING.md) - Resource planning
- [Toolchain files](../cmake/toolchains/) - Cross-compilation setup
