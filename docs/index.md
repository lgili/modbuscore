---
layout: default
title: Home
nav_order: 1
---

# ModbusCore v1.0
{: .fs-9 }

Modern, lightweight, dependency-injection-based Modbus protocol stack
{: .fs-6 .fw-300 }

[Get Started](#quick-start){: .btn .btn-primary .fs-5 .mb-4 .mb-md-0 .mr-2 }
[View on GitHub](https://github.com/lgili/modbuscore){: .btn .fs-5 .mb-4 .mb-md-0 }

---

## 🚀 What is ModbusCore v1.0?

**ModbusCore v1.0** is a complete rewrite of the Modbus protocol stack, designed with modern software architecture principles:

✅ **Dependency Injection** – Testable, modular, and flexible
✅ **Zero Dependencies** – Pure C11, runs anywhere
✅ **Heap-Free Operation** – Perfect for embedded systems
✅ **Non-Blocking I/O** – Cooperative multitasking friendly
✅ **Multiple Transports** – TCP, RTU, custom implementations
✅ **Fully Documented** – Comprehensive Doxygen API documentation

---

## ✨ Key Features

### 🎯 Protocol Support
- **Modbus TCP (MBAP)** – Full TCP/IP support with 7-byte header
- **Modbus RTU** – Serial communication (coming in Phase 5)
- **Function Codes**: FC03 (Read Holding), FC06 (Write Single), FC16 (Write Multiple)
- **Exception Handling** – Complete error response support

### 🏗️ Architecture
- **Runtime Builder Pattern** – Safe, incremental dependency configuration
- **Transport Abstraction** – Swap TCP, RTU, or mock transports without changing code
- **Protocol Engine FSM** – State machine with event telemetry
- **MBAP Framing** – Automatic TCP frame encoding/decoding

### 🔧 Developer Experience
- **Modern C11** – Clean, maintainable codebase
- **Doxygen Documentation** – Every function, struct, and enum documented
- **Example Code** – Complete working examples included
- **CMake Build System** – Easy integration and cross-platform builds

---

## 📦 Quick Start

### 1. Clone the Repository

```bash
git clone https://github.com/lgili/modbuscore.git
cd modbuscore
```

### 2. Build the Library

```bash
mkdir build && cd build
cmake ..
make
```

### 3. Run the Example

First, start a Modbus TCP server on port 5502:

```bash
# In tests directory
python3 simple_tcp_server.py
```

Then run the example client:

```bash
./build/tests/example_tcp_client_fc03
```

**Expected Output:**
```
=== ModbusCore v1.0 - TCP Client Example (FC03) ===

Step 1: Creating TCP transport...
✓ Connected to 127.0.0.1:5502

Step 2: Building runtime with DI...
✓ Runtime initialized

Step 3: Initializing protocol engine (client mode)...
✓ Engine ready

Step 4: Building FC03 request (unit=1, addr=0, count=10)...
✓ MBAP frame encoded (13 bytes: 7 MBAP + 6 PDU)
✓ Request submitted

Step 5: Polling for response (max 100 iterations)...
✓ Response received after 3 iterations!

Step 6: Parsing response...
  Registers read:
    [0] = 0x0000 (0)
    [1] = 0x0001 (1)
    [2] = 0x0002 (2)
    ...

=== SUCCESS ===
```

---

## 🎓 Tutorial: Your First Modbus TCP Client

Let's build a simple Modbus TCP client from scratch!

### Step 1: Include Headers

```c
#include <modbuscore/transport/posix_tcp.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>
```

### Step 2: Create TCP Transport

```c
// Configure TCP connection
mbc_posix_tcp_config_t tcp_config = {
    .host = "192.168.1.100",
    .port = 502,
    .connect_timeout_ms = 5000,
    .recv_timeout_ms = 1000,
};

// Create transport
mbc_transport_iface_t transport;
mbc_posix_tcp_ctx_t *tcp_ctx = NULL;

mbc_status_t status = mbc_posix_tcp_create(&tcp_config, &transport, &tcp_ctx);
if (!mbc_status_is_ok(status)) {
    fprintf(stderr, "Failed to connect\n");
    return 1;
}
```

### Step 3: Build Runtime with Dependency Injection

```c
// Initialize runtime builder
mbc_runtime_builder_t builder;
mbc_runtime_builder_init(&builder);
mbc_runtime_builder_with_transport(&builder, &transport);

// Build runtime (auto-fills clock, allocator, logger, diagnostics with defaults)
mbc_runtime_t runtime;
status = mbc_runtime_builder_build(&builder, &runtime);
```

### Step 4: Initialize Protocol Engine

```c
mbc_engine_t engine;
mbc_engine_config_t engine_config = {
    .runtime = &runtime,
    .role = MBC_ENGINE_ROLE_CLIENT,
    .framing = MBC_FRAMING_TCP,
    .response_timeout_ms = 3000,
};

status = mbc_engine_init(&engine, &engine_config);
```

### Step 5b: Stream Diagnostics Telemetry

For a concrete walkthrough of structured telemetry (Phase 7), compile and run:

```bash
cmake --build build --target modbus_tcp_diagnostics
./build/examples/modbus_tcp_diagnostics 127.0.0.1 1502
```

The program attempts a simple FC03 request and prints every diagnostics event emitted by the runtime and engine (state transitions, transport issues, timeouts, etc.). Point it at a live server to observe success traces or leave the port closed to see how failures surface with structured metadata.

### Step 5: Build and Send Request

```c
// Build FC03 request PDU
mbc_pdu_t request_pdu;
mbc_pdu_build_read_holding_request(&request_pdu,
    1,      // unit ID
    0,      // start address
    10      // quantity
);

// Wrap with MBAP header
mbc_mbap_header_t mbap_header = {
    .transaction_id = 1,
    .protocol_id = 0,
    .unit_id = 1
};

uint8_t request_buffer[256];
size_t request_length = 0;

// Encode MBAP frame
uint8_t pdu_buffer[256];
pdu_buffer[0] = request_pdu.function;
memcpy(&pdu_buffer[1], request_pdu.payload, request_pdu.payload_length);
size_t pdu_length = 1 + request_pdu.payload_length;

mbc_mbap_encode(&mbap_header, pdu_buffer, pdu_length,
                request_buffer, sizeof(request_buffer), &request_length);

// Submit request
mbc_engine_submit_request(&engine, request_buffer, request_length);
```

### Step 6: Poll for Response

```c
for (int i = 0; i < 100; ++i) {
    status = mbc_engine_step(&engine, 10);  // Process up to 10 bytes

    mbc_pdu_t response_pdu;
    if (mbc_engine_take_pdu(&engine, &response_pdu)) {
        // Got response!
        const uint8_t *register_data = NULL;
        size_t register_count = 0;

        mbc_pdu_parse_read_holding_response(&response_pdu,
                                            &register_data,
                                            &register_count);

        // Print registers
        for (size_t i = 0; i < register_count; ++i) {
            uint16_t value = (register_data[i * 2] << 8) | register_data[i * 2 + 1];
            printf("Register[%zu] = %u\n", i, value);
        }
        break;
    }
}
```

### Step 7: Cleanup

```c
mbc_engine_shutdown(&engine);
mbc_runtime_shutdown(&runtime);
mbc_posix_tcp_destroy(tcp_ctx);
```

**Complete example:** See [`tests/example_tcp_client_fc03.c`](https://github.com/lgili/modbuscore/blob/main/tests/example_tcp_client_fc03.c)

---

## 📚 Documentation

### Core Concepts

- [**Architecture Overview**](architecture.md) – Understand the layered design
- [**Dependency Injection**](dependency-injection.md) – Runtime builder pattern explained
- [**Transport Layer**](transports.md) – TCP, RTU, and custom transports
- [**Protocol Engine**](protocol-engine.md) – FSM, framing, and state management

### API Reference

- [**Status Codes**](api/status.md) – Error handling and return values
- [**PDU Functions**](api/pdu.md) – Building and parsing Protocol Data Units
- [**MBAP Framing**](api/mbap.md) – TCP frame encoding/decoding
- [**Runtime API**](api/runtime.md) – Dependency management
- [**Engine API**](api/engine.md) – Protocol state machine

### Guides

- [**Installation Guide**](guides/installation.md) – Building and integrating
- [**Porting Guide**](guides/porting.md) – Adding new transport layers
- [**Testing Guide**](guides/testing.md) – Running tests and integration tests
- [**Troubleshooting**](guides/troubleshooting.md) – Common issues and solutions

---

## 🏗️ Project Status

ModbusCore v1.0 is being developed in phases:

| Phase | Description | Status |
|-------|-------------|--------|
| **Phase 0** | Foundation (status codes, interfaces) | ✅ Complete |
| **Phase 1** | Runtime & Dependency Injection | ✅ Complete |
| **Phase 2** | PDU Helpers & Protocol Engine | ✅ Complete |
| **Phase 3** | Mock Transport & Testing | ✅ Complete |
| **Phase 4A** | POSIX TCP Driver | ✅ Complete |
| **Phase 4B** | MBAP Framing & Integration | ✅ Complete |
| **Phase 5** | RTU Support | 🚧 In Progress |
| **Phase 6** | Server Mode | 📋 Planned |
| **Phase 7** | Advanced Features (TLS, etc.) | 📋 Planned |

---

## 🤝 Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Follow the existing code style
4. Add tests for new features
5. Submit a pull request

See [CONTRIBUTING.md](https://github.com/lgili/modbuscore/blob/main/CONTRIBUTING.md) for details.

---

## 📄 License

MIT License - see [LICENSE](https://github.com/lgili/modbuscore/blob/main/LICENSE) for details.

---

## 🙏 Acknowledgments

Built with modern embedded systems best practices and inspired by:
- Clean Architecture principles
- Dependency Injection patterns
- Zero-dependency design philosophy

---

**Made with ❤️ for embedded systems developers**
