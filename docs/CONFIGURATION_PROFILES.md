# Configuration Profiles Guide

> **New in v1.1:** ModbusCore now supports simplified profile-based configuration!

## Quick Start

Instead of configuring dozens of individual flags, just choose a profile that matches your use case:

```c
#define MB_USE_PROFILE_SIMPLE    // For desktop/testing
#include <modbus/modbus.h>
```

That's it! The library is now configured with sensible defaults for your use case.

---

## Available Profiles

### 🖥️ SIMPLE - Desktop & Testing

**Target:** Desktop applications, CLI tools, quick prototypes, testing
**Memory:** ~85KB code, ~4KB RAM
**Features:** Everything enabled for maximum ease of use

**Best for:**
- Learning ModbusCore
- Quick prototypes
- Testing and development
- Desktop tools (Linux/macOS/Windows)

**Configuration:**
```c
#define MB_USE_PROFILE_SIMPLE
#include <modbus/modbus.h>
```

**Or via CMake:**
```bash
cmake --preset profile-simple
cmake --build --preset profile-simple
```

**Included Features:**
- ✅ Client & Server
- ✅ All transports (RTU, TCP, ASCII)
- ✅ All function codes (FC01-FC17)
- ✅ Full diagnostics with trace
- ✅ Power management
- ✅ libmodbus compatibility
- ✅ Generous buffers (512 bytes)
- ✅ Development assertions

---

### 🔌 EMBEDDED - MCU & IoT

**Target:** Microcontrollers, battery-powered devices, resource-constrained systems
**Memory:** ~26KB code, ~1.5KB RAM
**Features:** Essential only - optimized for minimal footprint

**Best for:**
- STM32, ESP32, nRF52 microcontrollers
- Battery-powered IoT sensors
- Resource-constrained embedded systems
- Production firmware

**Configuration:**
```c
#define MB_USE_PROFILE_EMBEDDED
#include <modbus/modbus.h>
```

**Or via CMake:**
```bash
cmake --preset profile-embedded
cmake --build --preset profile-embedded
```

**Included Features:**
- ✅ Client OR Server (choose one to save ~8KB)
- ✅ RTU transport only (saves ~6KB vs TCP)
- ✅ Essential function codes (FC01, FC03-06, FC0F, FC10)
- ✅ Basic counters (no trace to save RAM)
- ✅ Power management (CRITICAL for battery)
- ✅ ISR-safe mode for fast response
- ✅ Tight buffers (128 bytes)
- ❌ ASCII, TCP, advanced diagnostics disabled

**Customization:**
```c
#define MB_USE_PROFILE_EMBEDDED
/* Override defaults if needed */
#define MB_CONF_BUILD_SERVER 1       /* Enable server */
#define MB_CONF_BUILD_CLIENT 0       /* Disable client */
#include <modbus/modbus.h>
```

---

### 🏭 GATEWAY - Industrial Systems

**Target:** Protocol gateways, industrial PLCs, multi-device masters
**Memory:** ~75KB code, ~6KB RAM
**Features:** High performance, QoS, both client & server

**Best for:**
- Modbus RTU ↔ TCP gateways
- Industrial protocol converters
- Multi-slave master controllers
- High-throughput systems

**Configuration:**
```c
#define MB_USE_PROFILE_GATEWAY
#include <modbus/modbus.h>
```

**Or via CMake:**
```bash
cmake --preset profile-gateway
cmake --build --preset profile-gateway
```

**Included Features:**
- ✅ Client & Server (bridging)
- ✅ All transports (RTU, TCP, ASCII)
- ✅ All function codes
- ✅ QoS & priority queues (CRITICAL)
- ✅ Full diagnostics for monitoring
- ✅ Large buffers (512 bytes)
- ✅ 16 TCP connections
- ✅ Optimized for throughput
- ❌ Power management (gateways are always powered)

---

### 🚀 FULL - Maximum Features

**Target:** Development, testing, feature exploration
**Memory:** ~100KB code, ~8KB RAM
**Features:** Everything enabled

**Best for:**
- Exploring all library features
- Comprehensive testing
- Development and debugging
- When memory is not a constraint

**Configuration:**
```c
#define MB_USE_PROFILE_FULL
#include <modbus/modbus.h>
```

**Or via CMake:**
```bash
cmake --preset profile-full
cmake --build --preset profile-full
```

**Included Features:**
- ✅ Everything enabled
- ✅ Maximum diagnostics (256-entry trace)
- ✅ All advanced features
- ✅ Large buffers (1024 bytes)
- ✅ 32 TCP connections
- ✅ Compile-time info printing

---

## Comparison Table

| Feature | SIMPLE | EMBEDDED | GATEWAY | FULL |
|---------|--------|----------|---------|------|
| **Code Size** | ~85KB | ~26KB | ~75KB | ~100KB |
| **RAM Usage** | ~4KB | ~1.5KB | ~6KB | ~8KB |
| **Client** | ✅ | ✅ | ✅ | ✅ |
| **Server** | ✅ | ❌* | ✅ | ✅ |
| **RTU Transport** | ✅ | ✅ | ✅ | ✅ |
| **TCP Transport** | ✅ | ❌ | ✅ | ✅ |
| **ASCII Transport** | ✅ | ❌ | ✅ | ✅ |
| **All Function Codes** | ✅ | ❌† | ✅ | ✅ |
| **Diagnostics** | Full+Trace | Basic | Full+Trace | Max |
| **Power Management** | ✅ | ✅ | ❌ | ✅ |
| **QoS Queues** | ❌ | ❌ | ✅ | ✅ |
| **ISR Mode** | ❌ | ✅ | ❌ | ✅ |
| **Buffer Size** | 512B | 128B | 512B | 1024B |
| **TCP Connections** | 8 | 0 | 16 | 32 |

\* Can be enabled by defining `MB_CONF_BUILD_SERVER 1`
† Only essential FCs enabled (01, 03-06, 0F, 10)

---

## Usage Examples

### Method 1: Direct in Code

```c
// At the top of your main.c or before any modbus headers
#define MB_USE_PROFILE_SIMPLE
#include <modbus/modbus.h>

int main(void) {
    // Profile automatically configured!
    mb_client_t *client = mb_client_create_tcp("192.168.1.10:502");
    // ...
}
```

### Method 2: Via CMake

```bash
# Configure with profile
cmake --preset profile-simple -B build

# Build
cmake --build build

# Or combined
cmake --preset profile-simple && cmake --build --preset profile-simple
```

### Method 3: Via Compiler Flags

```bash
# GCC/Clang
gcc -DMB_USE_PROFILE_EMBEDDED mycode.c -lmodbus -o myapp

# CMake add_definitions
add_definitions(-DMB_USE_PROFILE_GATEWAY)
```

---

## Advanced: Custom Profile

If none of the predefined profiles fit your needs, you can create a custom configuration:

```c
#define MB_USE_PROFILE_CUSTOM
#include <modbus/profiles.h>

/* Now define everything manually */
#define MB_CONF_BUILD_CLIENT 1
#define MB_CONF_BUILD_SERVER 0
#define MB_CONF_TRANSPORT_RTU 1
#define MB_CONF_TRANSPORT_TCP 1
#define MB_CONF_ENABLE_FC03 1
#define MB_CONF_ENABLE_FC06 1
/* ... and so on ... */

#include <modbus/modbus.h>
```

---

## Migration from Legacy Configuration

If you have existing code with individual flags, it will continue to work:

```c
/* OLD WAY (still supported) */
#define MB_CONF_BUILD_CLIENT 1
#define MB_CONF_BUILD_SERVER 1
#define MB_CONF_TRANSPORT_RTU 1
#define MB_CONF_TRANSPORT_TCP 1
/* ... 50+ more lines ... */
#include <modbus/modbus.h>
```

To migrate to profiles:

1. **Identify your use case** - Which profile matches your needs?
2. **Replace all MB_CONF_* defines** with a single profile define
3. **Override specific settings** if needed (optional)

**Before (50+ lines):**
```c
#define MB_CONF_BUILD_CLIENT 1
#define MB_CONF_BUILD_SERVER 0
#define MB_CONF_TRANSPORT_RTU 1
#define MB_CONF_TRANSPORT_TCP 0
#define MB_CONF_ENABLE_FC01 1
#define MB_CONF_ENABLE_FC03 1
// ... 40+ more lines ...
```

**After (1 line):**
```c
#define MB_USE_PROFILE_EMBEDDED
```

---

## Troubleshooting

### Profile Not Applied

**Problem:** Settings don't match profile documentation

**Solution:** Ensure profile is defined BEFORE including any modbus headers:

```c
#define MB_USE_PROFILE_SIMPLE  // ✅ Before includes
#include <modbus/modbus.h>     // ✅ Correct

// NOT like this:
#include <modbus/modbus.h>     // ❌ Wrong
#define MB_USE_PROFILE_SIMPLE  // ❌ Too late
```

### Multiple Profiles Defined

**Problem:** Compile error: "Exactly ONE profile must be defined"

**Solution:** Check that only one profile is defined:

```c
// ❌ WRONG - Multiple profiles
#define MB_USE_PROFILE_SIMPLE
#define MB_USE_PROFILE_EMBEDDED

// ✅ CORRECT - Only one
#define MB_USE_PROFILE_SIMPLE
```

### Profile Info Not Printing

**Problem:** Can't see profile summary at compile time

**Solution:** Add `-DMB_PROFILE_PRINT_INFO` to compiler flags:

```bash
cmake --preset profile-full -DCMAKE_C_FLAGS="-DMB_PROFILE_PRINT_INFO"
```

Output:
```
ModbusCore Profile: FULL
  Description: Development - Everything enabled
  Estimated code size: ~100 KB
  Estimated RAM usage: ~8 KB
```

---

## CMake Integration

### Using in Your Project

```cmake
# CMakeLists.txt

# Method 1: Via target definition
add_executable(myapp main.c)
target_compile_definitions(myapp PRIVATE MB_USE_PROFILE_SIMPLE)
target_link_libraries(myapp PRIVATE modbuscore::modbuscore)

# Method 2: Via CMake variable
set(MB_PROFILE "EMBEDDED" CACHE STRING "ModbusCore profile")
add_executable(myapp main.c)
target_compile_definitions(myapp PRIVATE MB_USE_PROFILE_${MB_PROFILE})
target_link_libraries(myapp PRIVATE modbuscore::modbuscore)
```

### Preset Naming Convention

All profile presets follow the pattern `profile-<name>`:

- `profile-simple`
- `profile-embedded`
- `profile-gateway`
- `profile-full`

---

## Best Practices

1. **Start with SIMPLE** - Learn the library with maximum features enabled
2. **Optimize later** - Switch to EMBEDDED or GATEWAY when you understand your needs
3. **Don't over-customize** - Profiles are tested; custom configs may have issues
4. **Use CMake presets** - Easier to maintain and share across team
5. **Document your choice** - Add comment explaining why you chose a profile

---

## Next Steps

- **Quick Start:** See [examples/profile-demos/](../examples/profile-demos/) for working examples
- **API Guide:** Check [API_SIMPLIFIED.md](API_SIMPLIFIED.md) for the new simplified API
- **Migration:** Read [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md) for upgrading existing code

---

**Questions?** Open an issue on [GitHub](https://github.com/lgili/modbus/issues) or check [Discussions](https://github.com/lgili/modbus/discussions)
