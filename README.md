# ModbusCore v3.0

<div align="center">

**Modern, lightweight, dependency-injection-based Modbus protocol stack**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C Standard](https://img.shields.io/badge/C-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20Embedded-blue)](docs/quick-start.md)
[![Documentation](https://img.shields.io/badge/docs-github.io-green.svg)](https://lgili.github.io/modbuscore)

[Quick Start](docs/quick-start.md) • [Documentation](https://lgili.github.io/modbuscore) • [Examples](tests/example_tcp_client_fc03.c) • [CLI](docs/cli/modbus_cli_design.md) • [API Reference](docs/api/)

</div>

---

## ✨ What's New in v3.0?

ModbusCore v3.0 is a **complete rewrite** focused on modern software architecture:

- 🏗️ **Dependency Injection** – Testable, modular, flexible design
- 🧩 **Layered Architecture** – Clear separation of concerns
- 🔌 **Transport Abstraction** – Swap TCP, RTU, or custom implementations
- 📦 **Zero Dependencies** – Pure C11, no external libraries
- 🧪 **100% Testable** – Every layer tested in isolation
- 📚 **Fully Documented** – Comprehensive Doxygen API docs

---

## 🚀 Quick Example

```c
#include <modbuscore/transport/posix_tcp.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>

int main(void) {
    // 1. Create TCP transport
    mbc_posix_tcp_config_t tcp_config = {
        .host = "192.168.1.100",
        .port = 502,
        .connect_timeout_ms = 5000,
        .recv_timeout_ms = 1000,
    };
    mbc_transport_iface_t transport;
    mbc_posix_tcp_ctx_t *tcp_ctx;
    mbc_posix_tcp_create(&tcp_config, &transport, &tcp_ctx);

    // 2. Build runtime with DI
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &transport);
    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);

    // 3. Initialize protocol engine
    mbc_engine_t engine;
    mbc_engine_config_t engine_config = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,
        .response_timeout_ms = 3000,
    };
    mbc_engine_init(&engine, &engine_config);

    // 4. Read holding registers (FC03)
    mbc_pdu_t request;
    mbc_pdu_build_read_holding_request(&request, 1, 0, 10);

    // 5. Send request and receive response
    // ... (see full example in docs/quick-start.md)

    // 6. Cleanup
    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_posix_tcp_destroy(tcp_ctx);

    return 0;
}
```

**[📖 Full Tutorial](docs/quick-start.md)** • **[🎯 Complete Example](tests/example_tcp_client_fc03.c)**

---

## 🎯 Features

### Protocol Support

| Feature | Status | Notes |
|---------|--------|-------|
| **Modbus TCP** | ✅ Ready | Full MBAP support |
| **Modbus RTU** | 🚧 Phase 5 | Serial/RS-485 |
| **FC03** (Read Holding) | ✅ Ready | Client & Server |
| **FC06** (Write Single) | ✅ Ready | Client & Server |
| **FC16** (Write Multiple) | ✅ Ready | Client & Server |
| **Exception Responses** | ✅ Ready | Full support |
| **Server Mode** | 📋 Phase 6 | Planned |
| **TLS/Security** | 📋 Phase 7 | Planned |

### Architecture

- ✅ **Non-Blocking I/O** – Perfect for event loops
- ✅ **Heap-Free Operation** – Embedded-friendly
- ✅ **FSM-Based Engine** – Clear state management
- ✅ **Event Telemetry** – Debug and monitor engine state
- ✅ **Transport Layer Abstraction** – Easy to extend
- ✅ **Builder Pattern** – Safe dependency configuration

### Platform Support

| Platform | Status | Transport |
|----------|--------|-----------|
| Linux | ✅ Full | POSIX TCP, RTU (Phase 5) |
| macOS | ✅ Full | POSIX TCP, RTU (Phase 5) |
| Windows | ✅ Full | Winsock TCP, Win32 RTU (Phase 5) |
| Embedded | 📋 Planned | Custom transports |
| RTOS | 📋 Planned | FreeRTOS, Zephyr |

---

## 📦 Installation

### CMake (Recommended)

```bash
git clone https://github.com/lgili/modbuscore.git
cd modbuscore
mkdir build && cd build
cmake ..
make
```

### Add to Your Project

**Option 1: Git Submodule**
```bash
git submodule add https://github.com/lgili/modbuscore.git external/modbuscore
```

**Option 2: CMake FetchContent**
```cmake
include(FetchContent)
FetchContent_Declare(
    modbuscore
    GIT_REPOSITORY https://github.com/lgili/modbuscore.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(modbuscore)
target_link_libraries(your_app PRIVATE modbuscore)
```

**[📖 Full Installation Guide](docs/quick-start.md#installation)**

---

## 📚 Documentation

### Getting Started

- **[Quick Start Guide](docs/quick-start.md)** – Your first Modbus client in 5 minutes
- **[Architecture Overview](docs/architecture.md)** – Understand the design
- **[API Reference](docs/api/)** – Complete function documentation

### Core Concepts

- **[Dependency Injection](docs/dependency-injection.md)** – Runtime builder pattern
- **[Transport Layer](docs/transports.md)** – TCP, RTU, and custom transports
- **[Protocol Engine](docs/protocol-engine.md)** – FSM and state management
- **[PDU Handling](docs/api/pdu.md)** – Building and parsing requests/responses

### Guides

- **[Testing Guide](docs/guides/testing.md)** – Run tests and write your own
- **[Porting Guide](docs/guides/porting.md)** – Add new transport layers
- **[Troubleshooting](docs/guides/troubleshooting.md)** – Common issues

---

## 🏗️ Project Roadmap

| Phase | Description | Status |
|-------|-------------|--------|
| **Phase 0** | Foundation (status codes, interfaces) | ✅ Complete |
| **Phase 1** | Runtime & Dependency Injection | ✅ Complete |
| **Phase 2** | PDU Helpers & Protocol Engine | ✅ Complete |
| **Phase 3** | Mock Transport & Testing | ✅ Complete |
| **Phase 4A** | POSIX TCP Driver | ✅ Complete |
| **Phase 4B** | MBAP Framing & Integration | ✅ Complete |
| **Phase 5** | RTU Support (Serial) | 🚧 In Progress |
| **Phase 6** | Server Mode | 📋 Planned Q1 2025 |
| **Phase 7** | TLS, Security, Advanced Features | 📋 Planned Q2 2025 |

---

## 🧪 Testing

### Unit Tests

```bash
cd build
ctest --output-on-failure
```

**Test Coverage:**
- ✅ Runtime dependency injection
- ✅ PDU encoding/decoding
- ✅ Protocol engine FSM
- ✅ MBAP framing
- ✅ Transport layer abstraction

### Integration Tests

```bash
# Start test server
python3 tests/simple_tcp_server.py

# Run example client
./build/tests/example_tcp_client_fc03
```

---

## 🎓 Examples

### Included Examples

| Example | Description | Location |
|---------|-------------|----------|
| **TCP Client FC03** | Read holding registers | [`tests/example_tcp_client_fc03.c`](tests/example_tcp_client_fc03.c) |
| **Mock Transport Test** | Unit test example | [`tests/test_mock_transport.c`](tests/test_mock_transport.c) |
| **Protocol Engine Test** | FSM testing | [`tests/test_protocol_engine.c`](tests/test_protocol_engine.c) |

### Community Examples

*Coming soon! Submit your examples via PR.*

---

## 🤝 Contributing

We welcome contributions! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow the existing code style
4. Add tests for new features
5. Ensure all tests pass
6. Submit a pull request

**[📖 Contributing Guidelines](CONTRIBUTING.md)**

---

## 📖 API Documentation

### Core APIs

```c
// Status Codes
mbc_status_t status = function_call(...);
if (mbc_status_is_ok(status)) {
    // Success
}

// Runtime Builder (Dependency Injection)
mbc_runtime_builder_t builder;
mbc_runtime_builder_init(&builder);
mbc_runtime_builder_with_transport(&builder, &transport);
mbc_runtime_builder_build(&builder, &runtime);

// Protocol Engine
mbc_engine_init(&engine, &config);
mbc_engine_submit_request(&engine, buffer, length);
mbc_engine_step(&engine, budget);
mbc_engine_take_pdu(&engine, &pdu);

// PDU Builders
mbc_pdu_build_read_holding_request(&pdu, unit_id, address, quantity);
mbc_pdu_build_write_single_register(&pdu, unit_id, address, value);
mbc_pdu_build_write_multiple_registers(&pdu, unit_id, address, values, count);

// PDU Parsers
mbc_pdu_parse_read_holding_response(&pdu, &data, &count);
mbc_pdu_parse_write_single_response(&pdu, &address, &value);
mbc_pdu_parse_exception(&pdu, &function, &code);

// MBAP Framing (TCP)
mbc_mbap_encode(&header, pdu, pdu_len, buffer, buf_size, &out_len);
mbc_mbap_decode(buffer, length, &header, &pdu, &pdu_len);
mbc_mbap_expected_length(partial_buffer, partial_len);

// Transport Layer
mbc_posix_tcp_create(&config, &transport, &ctx);
mbc_transport_send(&transport, buffer, length, &io);
mbc_transport_receive(&transport, buffer, capacity, &io);
```

**[📖 Full API Reference](docs/api/)**

---

## 🔧 Build Options

```bash
# Debug build with sanitizers
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Release build (optimized)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build with tests
cmake -B build -DBUILD_TESTING=ON

# Build examples only
cmake -B build -DBUILD_EXAMPLES=ON -DBUILD_TESTING=OFF

# Cross-compile for embedded
cmake -B build -DCMAKE_TOOLCHAIN_FILE=path/to/toolchain.cmake
```

---

## 📄 License

MIT License - see [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

ModbusCore v3.0 is built with inspiration from:

- **Clean Architecture** – Robert C. Martin
- **Dependency Injection** – Design Patterns
- **Embedded Best Practices** – Industrial applications

Special thanks to all contributors and users of the previous versions.

---

## 📞 Support

- **Documentation**: [lgili.github.io/modbuscore](https://lgili.github.io/modbuscore)
- **Issues**: [GitHub Issues](https://github.com/lgili/modbuscore/issues)
- **Discussions**: [GitHub Discussions](https://github.com/lgili/modbuscore/discussions)

---

## 🌟 Star History

If you find ModbusCore useful, please consider giving it a star! ⭐

---

<div align="center">

**Made with ❤️ for embedded systems developers**

[Get Started](docs/quick-start.md) • [Documentation](https://lgili.github.io/modbuscore) • [Examples](tests/example_tcp_client_fc03.c)

</div>
