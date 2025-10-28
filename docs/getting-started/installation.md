# Installation Guide

Complete installation instructions for all platforms and package managers.

## Table of Contents

- [System Requirements](#system-requirements)
- [Quick Install](#quick-install)
- [Platform-Specific](#platform-specific-installation)
- [Package Managers](#package-manager-installation)
- [From Source](#building-from-source)
- [Verification](#verify-installation)

## System Requirements

### Minimum Requirements

| Component | Requirement |
|-----------|-------------|
| **C Standard** | C17 |
| **CMake** | 3.20 or higher |
| **Compiler** | GCC 9+, Clang 12+, or MSVC 2019+ |
| **Platform** | Linux, macOS, Windows, FreeRTOS, Zephyr |

### Optional Dependencies

| Feature | Requirement |
|---------|-------------|
| **Fuzzing** | Clang with LibFuzzer |
| **Testing** | CTest (included with CMake) |
| **Documentation** | Doxygen (optional) |

## Quick Install

### Linux / macOS

```bash
git clone https://github.com/lgili/modbuscore.git
cd modbuscore
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

### Windows (Command Prompt)

```cmd
git clone https://github.com/lgili/modbuscore.git
cd modbuscore
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build
```

### Windows (PowerShell)

```powershell
git clone https://github.com/lgili/modbuscore.git
cd modbuscore
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build
```

## Platform-Specific Installation

### Ubuntu / Debian

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake git

# Build and install
git clone https://github.com/lgili/modbuscore.git
cd modbuscore
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
```

### Fedora / RHEL / CentOS

```bash
# Install dependencies
sudo dnf install -y gcc cmake git

# Build and install
git clone https://github.com/lgili/modbuscore.git
cd modbuscore
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
```

### macOS (Homebrew)

```bash
# Install dependencies
brew install cmake

# Build and install
git clone https://github.com/lgili/modbuscore.git
cd modbuscore
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
sudo cmake --install build
```

### Windows (MSVC)

```cmd
REM Install Visual Studio 2019+ with C++ tools
REM Install CMake from https://cmake.org/download/

git clone https://github.com/lgili/modbuscore.git
cd modbuscore
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
cmake --install build
```

### Windows (MinGW)

```bash
# Using MSYS2
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake git

git clone https://github.com/lgili/modbuscore.git
cd modbuscore
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

## Package Manager Installation

### Conan

```bash
# Install Conan
pip install conan

# Create default profile
conan profile detect

# Use in your project
```

Create `conanfile.txt`:
```ini
[requires]
modbuscore/1.0.0

[generators]
CMakeDeps
CMakeToolchain
```

Install dependencies:
```bash
conan install . --build=missing
```

Use in `CMakeLists.txt`:
```cmake
find_package(ModbusCore REQUIRED CONFIG)
target_link_libraries(myapp ModbusCore::modbuscore)
```

### vcpkg

```bash
# Install vcpkg
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # or .\bootstrap-vcpkg.bat on Windows

# Install ModbusCore
./vcpkg install modbuscore
```

Use in CMake:
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
```

### pkg-config

After system installation:

```bash
# Check version
pkg-config --modversion modbuscore

# Get compile flags
pkg-config --cflags modbuscore

# Get linker flags
pkg-config --libs modbuscore

# Compile an application
gcc myapp.c $(pkg-config --cflags --libs modbuscore) -o myapp
```

## Building from Source

### Configuration Options

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \        # Build type: Debug, Release, RelWithDebInfo
    -DBUILD_TESTING=ON \                # Enable tests
    -DENABLE_FUZZING=OFF \              # Enable fuzzing (requires Clang)
    -DCMAKE_INSTALL_PREFIX=/usr/local   # Installation directory
```

### Build Types

| Type | Optimizations | Debug Info | Use Case |
|------|--------------|------------|----------|
| **Debug** | ❌ | ✅ | Development |
| **Release** | ✅ | ❌ | Production |
| **RelWithDebInfo** | ✅ | ✅ | Profiling |
| **MinSizeRel** | ✅ (size) | ❌ | Embedded |

### Custom Install Location

```bash
# Install to custom directory
cmake -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build
cmake --install build

# Use in your project
export PKG_CONFIG_PATH=$HOME/.local/lib/pkgconfig:$PKG_CONFIG_PATH
cmake -B build -DCMAKE_PREFIX_PATH=$HOME/.local
```

### Cross-Compilation

#### ARM Linux (Raspberry Pi)

```bash
# Install cross-compiler
sudo apt-get install -y gcc-arm-linux-gnueabihf

# Create toolchain file
cat > arm-toolchain.cmake << 'EOF'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF

# Build
cmake -B build-arm -DCMAKE_TOOLCHAIN_FILE=arm-toolchain.cmake
cmake --build build-arm
```

### Static vs Shared Library

By default, ModbusCore builds as a static library. To build shared:

```bash
cmake -B build -DBUILD_SHARED_LIBS=ON
cmake --build build
```

## Verify Installation

### Check Files

```bash
# Headers
ls /usr/local/include/modbuscore/

# Library
ls /usr/local/lib/libmodbuscore.a

# CMake package
ls /usr/local/lib/cmake/ModbusCore/

# pkg-config
ls /usr/local/lib/pkgconfig/modbuscore.pc
```

### Test Installation

Create `test.c`:
```c
#include <modbuscore/protocol/pdu.h>
#include <stdio.h>

int main(void) {
    mbc_pdu_t pdu = {0};
    if (mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10) == MBC_STATUS_OK) {
        printf("✓ ModbusCore is installed correctly!\n");
        return 0;
    }
    return 1;
}
```

Compile and run:
```bash
gcc test.c $(pkg-config --cflags --libs modbuscore) -o test
./test
```

Expected output:
```
✓ ModbusCore is installed correctly!
```

## Uninstall

```bash
# From build directory
sudo cmake --build build --target uninstall

# Or manually
sudo rm -rf /usr/local/include/modbuscore
sudo rm /usr/local/lib/libmodbuscore.a
sudo rm -rf /usr/local/lib/cmake/ModbusCore
sudo rm /usr/local/lib/pkgconfig/modbuscore.pc
```

## Troubleshooting

### CMake can't find ModbusCore

Set `CMAKE_PREFIX_PATH`:
```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/install
```

Or use environment variable:
```bash
export CMAKE_PREFIX_PATH=/path/to/install
cmake -B build
```

### Permission denied during install

Use `sudo` on Linux/macOS:
```bash
sudo cmake --install build
```

Or install to user directory:
```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --install build  # No sudo needed
```

### pkg-config doesn't find modbuscore

Add to `PKG_CONFIG_PATH`:
```bash
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
pkg-config --modversion modbuscore
```

### Compiler not found

Install build tools:

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential
```

**macOS:**
```bash
xcode-select --install
```

**Windows:**
Install [Visual Studio 2019+](https://visualstudio.microsoft.com/) with C++ tools

## Next Steps

- [Quick Start](quick-start.md) - Build your first app
- [First Application](first-application.md) - Complete TCP client
- [Examples](examples.md) - Ready-to-run examples

---

**Next**: [Quick Start →](quick-start.md) | [First Application →](first-application.md)
