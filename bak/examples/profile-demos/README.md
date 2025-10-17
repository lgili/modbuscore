# Profile Demos

This directory contains example programs demonstrating each ModbusCore configuration profile.

## Available Demos

### 1. simple_demo.c - SIMPLE Profile
**Use Case:** Desktop applications, testing, learning

**Features:**
- All features enabled
- Easy-to-use API
- Maximum simplicity

**Build & Run:**
```bash
cmake --preset profile-simple
cmake --build --preset profile-simple
./build/profile-simple/examples/profile-demos/simple_demo 127.0.0.1:502
```

**What it demonstrates:**
- Quick connection with `mb_host_tcp_connect()`
- Reading/writing registers
- Error handling
- Logging

---

### 2. embedded_demo.c - EMBEDDED Profile
**Use Case:** Microcontrollers, IoT devices, battery-powered systems

**Features:**
- Minimal footprint (~26KB code, ~1.5KB RAM)
- Essential features only
- Power management
- ISR-safe mode

**Build & Run:**
```bash
cmake --preset profile-embedded
cmake --build --preset profile-embedded
./build/profile-embedded/examples/profile-demos/embedded_demo /dev/ttyUSB0
```

**What it demonstrates:**
- Minimal resource usage
- RTU communication
- Power management hooks
- Memory footprint analysis

---

### 3. gateway_demo.c - GATEWAY Profile
**Use Case:** Industrial gateways, protocol converters

**Features:**
- Both client & server
- QoS with priority queues
- All transports
- High throughput

**Build & Run:**
```bash
cmake --preset profile-gateway
cmake --build --preset profile-gateway
./build/profile-gateway/examples/profile-demos/gateway_demo
```

**What it demonstrates:**
- Gateway architecture
- QoS configuration
- Protocol bridging concepts
- Performance characteristics

---

## Quick Comparison

| Demo | Profile | Code Size | RAM | Best For |
|------|---------|-----------|-----|----------|
| simple_demo | SIMPLE | ~85KB | ~4KB | Desktop, Learning |
| embedded_demo | EMBEDDED | ~26KB | ~1.5KB | MCU, IoT |
| gateway_demo | GATEWAY | ~75KB | ~6KB | Industrial |

---

## Building All Demos

```bash
# Build all profile demos at once
for profile in simple embedded gateway; do
    cmake --preset profile-$profile
    cmake --build --preset profile-$profile
done
```

---

## Next Steps

1. **Try SIMPLE first** - Start with `simple_demo.c` to learn the basics
2. **Optimize with EMBEDDED** - When ready for production MCU deployment
3. **Scale with GATEWAY** - For industrial multi-device scenarios

For full documentation, see:
- [Configuration Profiles Guide](../../docs/CONFIGURATION_PROFILES.md)
- [API Documentation](../../docs/API_REFERENCE.md)
