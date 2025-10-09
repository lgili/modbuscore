# Kconfig Configuration Guide

This guide explains how to configure ModbusCore using Kconfig configuration systems for Zephyr and ESP-IDF.

## Overview

Instead of manually editing `modbus_config.h`, you can use menu-driven configuration:
- **Zephyr**: Uses `Kconfig.modbus` with `west build -t menuconfig`
- **ESP-IDF**: Uses `Kconfig.projbuild` with `idf.py menuconfig`

Configuration changes automatically generate the correct build flags and defines.

---

## Zephyr Integration

### Setup

1. Add this library as a Zephyr module in your `west.yml`:
   ```yaml
   manifest:
     projects:
       - name: modbus
         url: https://github.com/yourusername/modbus
         path: modules/lib/modbus
   ```

2. In your application's `prj.conf`, enable the library:
   ```ini
   CONFIG_MODBUS=y
   ```

3. Run menuconfig to configure:
   ```bash
   west build -t menuconfig
   ```

### Configuration Menu Structure

Navigate to: **Modules → modbus → Modbus Protocol Stack**

```
├── Profile Selection (TINY/LEAN/FULL)
├── Mode Selection
│   ├── Enable Client
│   └── Enable Server
├── Transport Layer
│   ├── Enable RTU
│   ├── Enable TCP
│   └── Enable ASCII
├── Function Codes
│   ├── FC01: Read Coils
│   ├── FC02: Read Discrete Inputs
│   ├── FC03: Read Holding Registers
│   ├── FC04: Read Input Registers
│   ├── FC05: Write Single Coil
│   ├── FC06: Write Single Register
│   ├── FC15: Write Multiple Coils
│   ├── FC16: Write Multiple Registers
│   └── FC17: Read/Write Multiple Registers
├── Queue and Pool Settings
│   ├── Queue Depth (4-128)
│   ├── Max PDU Size (253-65535)
│   └── Pool Size (0-16)
├── Timeouts and Timing
│   ├── Default Response Timeout (ms)
│   ├── T1.5 Character Timeout Multiplier
│   └── T3.5 Frame Gap Multiplier
├── Diagnostics
│   ├── Enable Counters
│   ├── Counter Bit Width (16/32/64)
│   ├── Enable Trace Buffer
│   └── Trace Buffer Size
├── Advanced Features
│   ├── Power Management
│   ├── ISR-Safe Mode
│   ├── Zero-Copy I/O
│   └── Backpressure Control
└── Port Layer Selection (POSIX/Zephyr)
```

### Profile Presets

**TINY Profile** (~26KB ROM, 8KB RAM)
- Client OR Server (not both)
- Single transport (RTU recommended)
- Essential function codes only (FC03, FC06, FC16)
- Small queue depth (8 items)
- No diagnostics

**LEAN Profile** (~45KB ROM, 16KB RAM)
- Client + Server
- Two transports (RTU + TCP typical)
- Common function codes (FC01-FC06, FC15-FC16)
- Moderate queue depth (16 items)
- Basic diagnostics

**FULL Profile** (~85KB ROM, 32KB RAM)
- Client + Server
- All transports (RTU, TCP, ASCII)
- All function codes (FC01-FC17)
- Large queue depth (32 items)
- Full diagnostics with trace buffer

### Example Configurations

#### Minimal RTU Client (TINY)
```ini
CONFIG_MODBUS=y
CONFIG_MODBUS_PROFILE_TINY=y
CONFIG_MODBUS_ENABLE_CLIENT=y
CONFIG_MODBUS_ENABLE_SERVER=n
CONFIG_MODBUS_TRANSPORT_RTU=y
CONFIG_MODBUS_TRANSPORT_TCP=n
CONFIG_MODBUS_FC03=y
CONFIG_MODBUS_FC06=y
CONFIG_MODBUS_FC16=y
CONFIG_MODBUS_QUEUE_DEPTH=8
```

#### Balanced Gateway (LEAN)
```ini
CONFIG_MODBUS=y
CONFIG_MODBUS_PROFILE_LEAN=y
CONFIG_MODBUS_ENABLE_CLIENT=y
CONFIG_MODBUS_ENABLE_SERVER=y
CONFIG_MODBUS_TRANSPORT_RTU=y
CONFIG_MODBUS_TRANSPORT_TCP=y
CONFIG_MODBUS_FC01=y
CONFIG_MODBUS_FC03=y
CONFIG_MODBUS_FC05=y
CONFIG_MODBUS_FC06=y
CONFIG_MODBUS_FC15=y
CONFIG_MODBUS_FC16=y
CONFIG_MODBUS_QUEUE_DEPTH=16
```

#### Full-Featured Server (FULL)
```ini
CONFIG_MODBUS=y
CONFIG_MODBUS_PROFILE_FULL=y
CONFIG_MODBUS_ENABLE_CLIENT=y
CONFIG_MODBUS_ENABLE_SERVER=y
CONFIG_MODBUS_TRANSPORT_RTU=y
CONFIG_MODBUS_TRANSPORT_TCP=y
CONFIG_MODBUS_TRANSPORT_ASCII=y
CONFIG_MODBUS_FC01=y
CONFIG_MODBUS_FC02=y
CONFIG_MODBUS_FC03=y
CONFIG_MODBUS_FC04=y
CONFIG_MODBUS_FC05=y
CONFIG_MODBUS_FC06=y
CONFIG_MODBUS_FC15=y
CONFIG_MODBUS_FC16=y
CONFIG_MODBUS_FC17=y
CONFIG_MODBUS_QUEUE_DEPTH=32
CONFIG_MODBUS_DIAG_ENABLE_COUNTERS=y
CONFIG_MODBUS_DIAG_ENABLE_TRACE=y
CONFIG_MODBUS_DIAG_TRACE_SIZE=64
```

---

## ESP-IDF Integration

### Setup

1. Add this library as a component in your ESP-IDF project:
   ```bash
   cd components
   git clone https://github.com/yourusername/modbus modbus
   ```

2. Run menuconfig:
   ```bash
   idf.py menuconfig
   ```

### Configuration Menu

Navigate to: **Component config → Modbus Protocol Stack**

The menu structure mirrors Zephyr but uses `MB_CONF_*` prefix:

```
Component config → Modbus Protocol Stack
├── Profile Selection (TINY/LEAN/FULL)
├── Mode Selection
│   ├── Enable Client
│   └── Enable Server
├── Transport Layer
│   ├── Enable RTU
│   ├── Enable TCP
│   └── Enable ASCII
├── Function Codes (FC01-FC17)
├── Queue and Pool Settings
├── Timeouts and Timing
├── Diagnostics
└── Advanced Features
```

### Example ESP32-C3 Configuration

#### WiFi-to-RTU Gateway (LEAN)
```ini
# Component config → Modbus Protocol Stack
CONFIG_MB_CONF_PROFILE_LEAN=y
CONFIG_MB_CONF_ENABLE_CLIENT=y
CONFIG_MB_CONF_ENABLE_SERVER=y
CONFIG_MB_CONF_TRANSPORT_RTU=y
CONFIG_MB_CONF_TRANSPORT_TCP=y
CONFIG_MB_CONF_TRANSPORT_ASCII=n
CONFIG_MB_CONF_FC01=y
CONFIG_MB_CONF_FC03=y
CONFIG_MB_CONF_FC05=y
CONFIG_MB_CONF_FC06=y
CONFIG_MB_CONF_FC15=y
CONFIG_MB_CONF_FC16=y
CONFIG_MB_CONF_QUEUE_DEPTH=16
CONFIG_MB_CONF_RESPONSE_TIMEOUT_MS=1000
```

---

## Configuration Options Reference

### Profile Selection

| Profile | ROM Usage | RAM Usage | Use Case |
|---------|-----------|-----------|----------|
| TINY    | ~26KB     | ~8KB      | Single-purpose devices, battery-powered |
| LEAN    | ~45KB     | ~16KB     | Balanced devices, gateways |
| FULL    | ~85KB     | ~32KB     | Feature-rich devices, diagnostics needed |

### Queue Depth Guidelines

| Queue Depth | RAM Impact | Use Case |
|-------------|------------|----------|
| 4-8         | ~2KB       | Single connection, low traffic |
| 16          | ~4KB       | Multiple connections, moderate traffic |
| 32-64       | ~8-16KB    | High-traffic gateway, burst handling |
| 128         | ~32KB      | Enterprise gateway, heavy concurrent load |

### Timeout Configuration

**Response Timeout** (500-10000ms)
- Low (500ms): Local wired networks, fast devices
- Medium (1000ms): Typical industrial networks
- High (3000-5000ms): Slow devices, wireless links
- Very High (10000ms): Satellite/cellular, congested networks

**T1.5 Character Timeout Multiplier** (1-5)
- 1: Standard timing (default)
- 2-3: Noisy lines, slow UARTs
- 4-5: Very problematic environments

**T3.5 Frame Gap Multiplier** (1-5)
- 1: Standard timing (default)
- 2-3: Devices with slow turnaround
- 4-5: Legacy devices with buffering issues

### Diagnostics Configuration

**Counters** (16/32/64-bit)
- 16-bit: Minimal RAM (~20 bytes), suitable for short-term stats
- 32-bit: Balanced (~40 bytes), typical choice
- 64-bit: Long-running devices (~80 bytes), never overflows

**Trace Buffer** (16-128 entries)
- 16: Minimal debugging (~1KB)
- 32: Typical debugging (~2KB)
- 64-128: Deep debugging (~4-8KB)

---

## Advanced Features

### Power Management
Enable for battery-powered devices. Library will:
- Release CPU during blocking waits
- Notify application before long operations
- Support wake-on-packet (if hardware supports)

**When to enable:**
- Battery-powered sensors
- Solar-powered devices
- Energy-harvesting applications

### ISR-Safe Mode
Enable for real-time systems. Library will:
- Use ISR-safe primitives only
- No blocking calls in critical paths
- Deterministic timing

**When to enable:**
- Hard real-time requirements
- Callback from ISR needed
- Deterministic response time required

### Zero-Copy I/O
Enable for high-throughput applications. Library will:
- Avoid internal buffering where possible
- Use scatter-gather I/O
- Reduce memory copies

**When to enable:**
- High-speed gateways
- Large data transfers
- CPU-constrained systems

### Backpressure Control
Enable for congested networks. Library will:
- Apply flow control
- Reject new requests when queue full
- Provide queue status to application

**When to enable:**
- Servers with many clients
- Unreliable networks
- Resource-constrained devices

---

## Build Integration

### CMake Variables from Kconfig

Kconfig options automatically generate CMake cache variables:

| Kconfig Option | CMake Variable | Values |
|----------------|----------------|--------|
| `CONFIG_MODBUS_PROFILE_*` | `MODBUS_PROFILE` | TINY/LEAN/FULL |
| `CONFIG_MODBUS_ENABLE_CLIENT` | `MODBUS_ENABLE_CLIENT` | ON/OFF |
| `CONFIG_MODBUS_ENABLE_SERVER` | `MODBUS_ENABLE_SERVER` | ON/OFF |
| `CONFIG_MODBUS_TRANSPORT_RTU` | `MODBUS_TRANSPORT_RTU` | ON/OFF |
| `CONFIG_MODBUS_TRANSPORT_TCP` | `MODBUS_TRANSPORT_TCP` | ON/OFF |
| `CONFIG_MODBUS_FC*` | `MODBUS_FC*` | ON/OFF |

### Verification

After configuration, verify the generated settings:

**Zephyr:**
```bash
west build -t menuconfig
# Check: Modules → modbus → Modbus Protocol Stack
```

**ESP-IDF:**
```bash
idf.py menuconfig
# Check: Component config → Modbus Protocol Stack
```

**Build and check defines:**
```bash
# Zephyr
west build -v 2>&1 | grep -E "CONFIG_MODBUS_|MB_CONF_"

# ESP-IDF
idf.py build -v 2>&1 | grep -E "CONFIG_MB_CONF_"
```

---

## Troubleshooting

### Configuration Not Applied

**Problem:** Changes in menuconfig don't affect the build.

**Solutions:**
1. Clean and rebuild:
   ```bash
   west build -t pristine  # Zephyr
   idf.py fullclean        # ESP-IDF
   ```

2. Check `prj.conf` or `sdkconfig`:
   ```bash
   grep MODBUS prj.conf    # Zephyr
   grep MB_CONF sdkconfig  # ESP-IDF
   ```

3. Verify module is enabled in CMakeLists.txt

### Conflicts Between Options

**Problem:** Incompatible options selected (e.g., TINY + all function codes).

**Solution:** Kconfig will show warnings. Follow the dependency rules:
- TINY profile limits available options
- Some features require specific transports
- Diagnostics require LEAN or FULL profile

### Build Errors After Configuration

**Problem:** Build fails with undefined references.

**Solution:**
1. Check that required source files are included in CMakeLists.txt
2. Verify toolchain supports selected features
3. Check that port layer is correctly configured

### Memory Overflow

**Problem:** Device runs out of RAM/ROM.

**Solution:**
1. Switch to smaller profile (FULL → LEAN → TINY)
2. Disable unused function codes
3. Reduce queue depth
4. Disable diagnostics
5. Use `MinSizeRel` build type

---

## Best Practices

1. **Start with a profile** - Choose TINY/LEAN/FULL based on your constraints
2. **Enable only what you need** - Each feature adds footprint
3. **Test incrementally** - Enable features one at a time
4. **Monitor footprint** - Use `scripts/ci/report_footprint.py` to track size
5. **Use diagnostics in development** - Disable in production for TINY/LEAN
6. **Document your configuration** - Save your `prj.conf` or `sdkconfig`

---

## See Also

- [CMAKE_PRESETS_GUIDE.md](CMAKE_PRESETS_GUIDE.md) - CMake preset usage
- [FOOTPRINT.md](FOOTPRINT.md) - Memory usage analysis
- [RESOURCE_PLANNING.md](RESOURCE_PLANNING.md) - Resource planning guide
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - Common issues
