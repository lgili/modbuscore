# ModbusCore# ModbusCore v1.0



<div align="center"><div align="center">



**Modern, testable Modbus library with dependency injection****Modern, lightweight, dependency-injection-based Modbus protocol stack**



[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

[![C Standard](https://img.shields.io/badge/C-C17-blue.svg)](https://en.wikipedia.org/wiki/C17_(C_standard_revision))[![C Standard](https://img.shields.io/badge/C-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))

[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C.svg)](https://cmake.org/)[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20Embedded-blue)](docs/quick-start.md)

[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)](https://github.com/lgili/modbuscore)[![Documentation](https://img.shields.io/badge/docs-github.io-green.svg)](https://lgili.github.io/modbuscore)



[Quick Start](#-quick-start) • [Features](#-features) • [Installation](#-installation) • [Documentation](docs/) • [Support](SUPPORT.md)[Quick Start](docs/quick-start.md) • [Documentation](https://lgili.github.io/modbuscore) • [Examples](tests/example_tcp_client_fc03.c) • [CLI](docs/cli/modbus_cli_design.md) • [API Reference](docs/api/)



</div></div>



------



## 🚀 Quick Start## ✨ What's New in v1.0?



```cModbusCore v1.0 is a **complete rewrite** focused on modern software architecture:

#include <modbuscore/protocol/pdu.h>

#include <modbuscore/protocol/engine.h>- 🏗️ **Dependency Injection** – Testable, modular, flexible design

#include <modbuscore/transport/posix_tcp.h>- 🧩 **Layered Architecture** – Clear separation of concerns

- 🔌 **Transport Abstraction** – Swap TCP, RTU, or custom implementations

int main(void) {- 📦 **Zero Dependencies** – Pure C11, no external libraries

    // Create runtime- 🧪 **100% Testable** – Every layer tested in isolation

    mbc_runtime_t runtime = {0};- 📚 **Fully Documented** – Comprehensive Doxygen API docs

    mbc_runtime_init(&runtime, &(mbc_runtime_config_t){

        .transport = mbc_posix_tcp_create("192.168.1.100", 502),---

        // ... other dependencies

    });## 🚀 Quick Example

    

    // Build request```c

    mbc_pdu_t request = {0};#include <modbuscore/transport/posix_tcp.h>

    mbc_pdu_build_read_holding_request(&request, 1, 0, 10);#include <modbuscore/runtime/builder.h>

    #include <modbuscore/protocol/engine.h>

    // Send and receive#include <modbuscore/protocol/pdu.h>

    mbc_engine_t engine = {0};#include <modbuscore/protocol/mbap.h>

    mbc_engine_init(&engine, &runtime, MBC_ENGINE_ROLE_CLIENT);

    mbc_engine_submit_request(&engine, &request);int main(void) {

        // 1. Create TCP transport

    // Process response...    mbc_posix_tcp_config_t tcp_config = {

    return 0;        .host = "192.168.1.100",

}        .port = 502,

```        .connect_timeout_ms = 5000,

        .recv_timeout_ms = 1000,

## ✨ Features    };

    mbc_transport_iface_t transport;

- 🔌 **Pluggable Architecture** - Swap transports, clocks, allocators without code changes    mbc_posix_tcp_ctx_t *tcp_ctx;

- 🧪 **100% Testable** - Everything is mockable via dependency injection    mbc_posix_tcp_create(&tcp_config, &transport, &tcp_ctx);

- 🌍 **Cross-Platform** - Linux, macOS, Windows, FreeRTOS, Zephyr, bare metal

- 🔒 **Production-Ready** - Fuzzed with LibFuzzer (1B+ executions), auto-heal with circuit breaker    // 2. Build runtime with DI

- 📦 **Multiple Package Managers** - CMake, pkg-config, Conan, vcpkg    mbc_runtime_builder_t builder;

- 🛡️ **Memory Safe** - No hidden allocations, explicit ownership, RAII where applicable    mbc_runtime_builder_init(&builder);

    mbc_runtime_builder_with_transport(&builder, &transport);

### Supported Protocols & Transports    mbc_runtime_t runtime;

    mbc_runtime_builder_build(&builder, &runtime);

| Protocol | Status | Transport | Status |

|----------|--------|-----------|--------|    // 3. Initialize protocol engine

| Modbus TCP (MBAP) | ✅ | POSIX TCP | ✅ |    mbc_engine_t engine;

| Modbus RTU | ✅ | Windows TCP | ✅ |    mbc_engine_config_t engine_config = {

| Function Codes | FC03, FC06, FC16 | POSIX Serial | ✅ |        .runtime = &runtime,

| | | Windows Serial | ✅ |        .role = MBC_ENGINE_ROLE_CLIENT,

| | | Bare Metal UART | ✅ |        .framing = MBC_FRAMING_TCP,

| | | Mock (Testing) | ✅ |        .response_timeout_ms = 3000,

    };

## 📦 Installation    mbc_engine_init(&engine, &engine_config);



### Using CMake    // 4. Read holding registers (FC03)

    mbc_pdu_t request;

```bash    mbc_pdu_build_read_holding_request(&request, 1, 0, 10);

cmake -B build -DCMAKE_BUILD_TYPE=Release

cmake --build build    // 5. Send request and receive response

sudo cmake --install build    // ... (see full example in docs/quick-start.md)

```

    // 6. Cleanup

In your project:    mbc_engine_shutdown(&engine);

    mbc_runtime_shutdown(&runtime);

```cmake    mbc_posix_tcp_destroy(tcp_ctx);

find_package(ModbusCore REQUIRED)

target_link_libraries(myapp ModbusCore::modbuscore)    return 0;

```}

```

### Using pkg-config

**[📖 Full Tutorial](docs/quick-start.md)** • **[🎯 Complete Example](tests/example_tcp_client_fc03.c)**

```bash

gcc myapp.c $(pkg-config --cflags --libs modbuscore) -o myapp---

```

## 🎯 Features

### Using Conan

### Protocol Support

```ini

[requires]| Feature | Status | Notes |

modbuscore/1.0.0|---------|--------|-------|

| **Modbus TCP** | ✅ Ready | Full MBAP support |

[generators]| **Modbus RTU** | 🚧 Phase 5 | Serial/RS-485 |

CMakeDeps| **FC03** (Read Holding) | ✅ Ready | Client & Server |

CMakeToolchain| **FC06** (Write Single) | ✅ Ready | Client & Server |

```| **FC16** (Write Multiple) | ✅ Ready | Client & Server |

| **Exception Responses** | ✅ Ready | Full support |

### Using vcpkg| **Server Mode** | 📋 Phase 6 | Planned |

| **TLS/Security** | 📋 Phase 7 | Planned |

```bash

vcpkg install modbuscore### Architecture

```

- ✅ **Non-Blocking I/O** – Perfect for event loops

## 🎯 Examples- ✅ **Heap-Free Operation** – Embedded-friendly

- ✅ **FSM-Based Engine** – Clear state management

### TCP Client (Read Holding Registers)- ✅ **Event Telemetry** – Debug and monitor engine state

- ✅ **Transport Layer Abstraction** – Easy to extend

```c- ✅ **Builder Pattern** – Safe dependency configuration

#include <modbuscore/runtime/runtime.h>

#include <modbuscore/protocol/engine.h>### Platform Support

#include <modbuscore/protocol/pdu.h>

#include <modbuscore/transport/posix_tcp.h>| Platform | Status | Transport |

|----------|--------|-----------|

int main(void) {| Linux | ✅ Full | POSIX TCP, RTU (Phase 5) |

    // Create TCP transport| macOS | ✅ Full | POSIX TCP, RTU (Phase 5) |

    mbc_posix_tcp_config_t tcp_config = {| Windows | ✅ Full | Winsock TCP, Win32 RTU (Phase 5) |

        .hostname = "192.168.1.100",| Embedded | 📋 Planned | Custom transports |

        .port = 502,| RTOS | 📋 Planned | FreeRTOS, Zephyr |

        .connect_timeout_ms = 5000,

    };---

    

    mbc_runtime_t runtime = {0};## 📦 Installation

    mbc_runtime_config_t config = {

        .transport = *mbc_posix_tcp_create(&runtime, &tcp_config),### CMake (Recommended)

        // Use default clock, allocator, logger

    };```bash

    mbc_runtime_init(&runtime, &config);git clone https://github.com/lgili/modbuscore.git

    cd modbuscore

    // Create client enginemkdir build && cd build

    mbc_engine_t engine = {0};cmake ..

    mbc_engine_config_t engine_config = {make

        .role = MBC_ENGINE_ROLE_CLIENT,```

        .response_timeout_ms = 1000,

    };### Add to Your Project

    mbc_engine_init(&engine, &runtime, &engine_config);

    **Option 1: Git Submodule**

    // Build and send request```bash

    mbc_pdu_t request = {0};git submodule add https://github.com/lgili/modbuscore.git external/modbuscore

    mbc_pdu_build_read_holding_request(&request, 1, 0, 10); // Read 10 registers from address 0```

    

    mbc_status_t status = mbc_engine_submit_request(&engine, &request);**Option 2: CMake FetchContent**

    if (status != MBC_STATUS_OK) {```cmake

        fprintf(stderr, "Failed to submit request\n");include(FetchContent)

        return 1;FetchContent_Declare(

    }    modbuscore

        GIT_REPOSITORY https://github.com/lgili/modbuscore.git

    // Poll for response    GIT_TAG        main

    mbc_pdu_t response = {0};)

    while (mbc_engine_step(&engine, 100) == MBC_STATUS_OK) {FetchContent_MakeAvailable(modbuscore)

        if (mbc_engine_poll_response(&engine, &response) == MBC_STATUS_OK) {target_link_libraries(your_app PRIVATE modbuscore)

            printf("✓ Received %u bytes\n", response.payload_length);```

            break;

        }**[📖 Full Installation Guide](docs/quick-start.md#installation)**

    }

    ---

    // Cleanup

    mbc_transport_close(&runtime.deps.transport);## 📚 Documentation

    return 0;

}### Getting Started

```

- **[Quick Start Guide](docs/quick-start.md)** – Your first Modbus client in 5 minutes

### RTU Server (Loop Device)- **[Architecture Overview](docs/architecture.md)** – Understand the design

- **[API Reference](docs/api/)** – Complete function documentation

```c

#include <modbuscore/protocol/engine.h>### Core Concepts

#include <modbuscore/transport/posix_rtu.h>

- **[Dependency Injection](docs/dependency-injection.md)** – Runtime builder pattern

int main(void) {- **[Transport Layer](docs/transports.md)** – TCP, RTU, and custom transports

    mbc_posix_rtu_config_t rtu_config = {- **[Protocol Engine](docs/protocol-engine.md)** – FSM and state management

        .device = "/dev/ttyUSB0",- **[PDU Handling](docs/api/pdu.md)** – Building and parsing requests/responses

        .baudrate = 9600,

        .parity = 'N',### Guides

        .data_bits = 8,

        .stop_bits = 1,- **[Testing Guide](docs/guides/testing.md)** – Run tests and write your own

    };- **[Porting Guide](docs/guides/porting.md)** – Add new transport layers

    - **[Troubleshooting](docs/guides/troubleshooting.md)** – Common issues

    mbc_runtime_t runtime = {0};

    mbc_runtime_init(&runtime, &(mbc_runtime_config_t){---

        .transport = *mbc_posix_rtu_create(&runtime, &rtu_config),

    });## 🏗️ Project Roadmap

    

    mbc_engine_t engine = {0};| Phase | Description | Status |

    mbc_engine_init(&engine, &runtime, &(mbc_engine_config_t){|-------|-------------|--------|

        .role = MBC_ENGINE_ROLE_SERVER,| **Phase 0** | Foundation (status codes, interfaces) | ✅ Complete |

    });| **Phase 1** | Runtime & Dependency Injection | ✅ Complete |

    | **Phase 2** | PDU Helpers & Protocol Engine | ✅ Complete |

    // Server loop| **Phase 3** | Mock Transport & Testing | ✅ Complete |

    while (running) {| **Phase 4A** | POSIX TCP Driver | ✅ Complete |

        mbc_engine_step(&engine, 100);| **Phase 4B** | MBAP Framing & Integration | ✅ Complete |

    }| **Phase 5** | RTU Support (Serial) | 🚧 In Progress |

    | **Phase 6** | Server Mode | 📋 Planned Q1 2025 |

    return 0;| **Phase 7** | TLS, Security, Advanced Features | 📋 Planned Q2 2025 |

}

```---



### With Auto-Heal (Automatic Retry)## 🧪 Testing



```c### Unit Tests

#include <modbuscore/runtime/autoheal.h>

```bash

mbc_autoheal_config_t heal_config = {cd build

    .runtime = &runtime,ctest --output-on-failure

    .max_retries = 5,```

    .initial_backoff_ms = 100,

    .max_backoff_ms = 5000,**Test Coverage:**

    .circuit_cooldown_ms = 10000,- ✅ Runtime dependency injection

};- ✅ PDU encoding/decoding

- ✅ Protocol engine FSM

mbc_autoheal_supervisor_t* supervisor = mbc_autoheal_supervisor_create(&heal_config);- ✅ MBAP framing

- ✅ Transport layer abstraction

// Submit request - supervisor handles retries automatically

mbc_autoheal_supervisor_submit(supervisor, &request);### Integration Tests

```

```bash

More examples: [`examples/`](examples/)# Start test server

python3 tests/simple_tcp_server.py

## 🧪 Testing

# Run example client

```bash./build/tests/example_tcp_client_fc03

# Build and run tests```

cmake -B build -DBUILD_TESTING=ON

cmake --build build---

ctest --test-dir build --output-on-failure

## 🧰 Code Style & Lint

# Run fuzzing (requires Clang)

cmake -B build -DENABLE_FUZZING=ON -DCMAKE_C_COMPILER=clangUse the provided LLVM-based configuration before committing changes:

cmake --build build

./build/tests/fuzz/fuzz_mbap_decoder corpus/```bash

# format C sources/headers

# Or use Docker (macOS)clang-format -i src/protocol/engine.c include/modbuscore/protocol/engine.h

./scripts/fuzz_in_docker.sh 300  # 5 minutes

```# run clang-tidy analysis (add include paths after `--`)

clang-tidy src/protocol/engine.c -- -Iinclude -std=c17

## 📚 Documentation```



| Document | Description |Ambos usam as configurações de `.clang-format` e `.clang-tidy`. A pipeline de CI

|----------|-------------|também consome esses arquivos, então rodar localmente evita divergências.

| [Quick Start](docs/QUICKSTART.md) | Get started in 5 minutes |

| [Architecture](docs/ARCHITECTURE.md) | System design and patterns |### Makefile Shortcuts

| [API Reference](docs/api/) | Complete API documentation |

| [Fuzzing Guide](docs/FUZZING.md) | Fuzzing infrastructure |Um `Makefile` na raiz oferece alvos úteis:

| [Troubleshooting](docs/TROUBLESHOOTING.md) | Common issues and solutions |

| [Roadmap](docs/roadmap.md) | Development roadmap |```bash

| [Changelog](CHANGELOG.md) | Version history |# configure + build (alvo padrão)

| [Versioning Policy](VERSIONING.md) | SemVer and LTS support |make

| [Support](SUPPORT.md) | Getting help |

# lint/format

## 🛠️ Requirementsmake format

make tidy

### Minimum

# rodar ctest com output detalhado

- **C Standard**: C17make test

- **CMake**: 3.20+

- **Compiler**: GCC 9+, Clang 12+, MSVC 2019+# compilar todos os exemplos

make examples

### Optional```



- **Fuzzing**: Clang with LibFuzzer supportUse `make help` para listar todos os alvos e sobrescreva ferramentas conforme

- **Conan**: 2.0+ for package managementnecessário, por exemplo `make PYTHON=python` ou `make CLANG_TIDY=run-clang-tidy`.

- **vcpkg**: Latest for package management

---

### Platform Support

## 🎓 Examples

| Platform | Status | Notes |

|----------|--------|-------|### Included Examples

| Linux (glibc) | ✅ Tier 1 | Tested in CI |

| Linux (musl) | ✅ Tier 1 | Alpine Linux || Example | Description | Location |

| macOS 11+ | ✅ Tier 1 | Intel & Apple Silicon ||---------|-------------|----------|

| Windows 10+ | ✅ Tier 1 | MSVC & MinGW || **TCP Client FC03** | Read holding registers | [`tests/example_tcp_client_fc03.c`](tests/example_tcp_client_fc03.c) |

| FreeRTOS | ⚙️ Tier 2 | Community ports || **Mock Transport Test** | Unit test example | [`tests/test_mock_transport.c`](tests/test_mock_transport.c) |

| Zephyr | ⚙️ Tier 2 | Community ports || **Protocol Engine Test** | FSM testing | [`tests/test_protocol_engine.c`](tests/test_protocol_engine.c) |

| Bare Metal | ⚙️ Tier 3 | Bring your own UART |

### Community Examples

## 🤝 Contributing

*Coming soon! Submit your examples via PR.*

Contributions welcome! See [CONTRIBUTING.md](docs/CONTRIBUTING.md) for guidelines.

---

```bash

# Quick contribution workflow## 🤝 Contributing

git clone https://github.com/lgili/modbuscore.git

cd modbuscoreWe welcome contributions! Please:

cmake -B build -DBUILD_TESTING=ON

cmake --build build1. Fork the repository

ctest --test-dir build2. Create a feature branch (`git checkout -b feature/amazing-feature`)

# Make your changes, add tests3. Follow the existing code style

clang-format -i src/**/*.c include/**/*.h4. Add tests for new features

git commit -m "feat: add awesome feature"5. Ensure all tests pass

git push origin feature/awesome-feature6. Submit a pull request

```

**[📖 Contributing Guidelines](CONTRIBUTING.md)**

## 📄 License

---

ModbusCore is licensed under the [MIT License](LICENSE).

## 📖 API Documentation

```

Copyright (c) 2025 Lucas Gili### Core APIs



Permission is hereby granted, free of charge, to any person obtaining a copy...```c

```// Status Codes

mbc_status_t status = function_call(...);

## 🌟 Why ModbusCore?if (mbc_status_is_ok(status)) {

    // Success

### vs. libmodbus}



| Feature | ModbusCore | libmodbus |// Runtime Builder (Dependency Injection)

|---------|-----------|-----------|mbc_runtime_builder_t builder;

| Dependency Injection | ✅ | ❌ |mbc_runtime_builder_init(&builder);

| Testability | ✅ 100% mockable | ⚠️ Limited |mbc_runtime_builder_with_transport(&builder, &transport);

| Modern C | ✅ C17 | ⚠️ C99 |mbc_runtime_builder_build(&builder, &runtime);

| Async/Non-blocking | ✅ | ❌ Blocking |

| Embedded Support | ✅ Bare metal ready | ⚠️ POSIX only |// Protocol Engine

| Auto-heal | ✅ Built-in | ❌ |mbc_engine_init(&engine, &config);

| Fuzzing | ✅ 1B+ execs | ❌ |mbc_engine_submit_request(&engine, buffer, length);

mbc_engine_step(&engine, budget);

### Design Philosophymbc_engine_take_pdu(&engine, &pdu);



1. **Explicit over Implicit** - No hidden global state// PDU Builders

2. **Testability First** - Everything is mockablembc_pdu_build_read_holding_request(&pdu, unit_id, address, quantity);

3. **Zero-Cost Abstractions** - Pay only for what you usembc_pdu_build_write_single_register(&pdu, unit_id, address, value);

4. **Fail Fast** - Errors are explicit, not hiddenmbc_pdu_build_write_multiple_registers(&pdu, unit_id, address, values, count);

5. **Portable** - Write once, run anywhere

// PDU Parsers

## 🔗 Linksmbc_pdu_parse_read_holding_response(&pdu, &data, &count);

mbc_pdu_parse_write_single_response(&pdu, &address, &value);

- **Homepage**: [modbuscore.dev](https://modbuscore.dev) (coming soon)mbc_pdu_parse_exception(&pdu, &function, &code);

- **GitHub**: [github.com/lgili/modbuscore](https://github.com/lgili/modbuscore)

- **Issues**: [github.com/lgili/modbuscore/issues](https://github.com/lgili/modbuscore/issues)// MBAP Framing (TCP)

- **Discussions**: [github.com/lgili/modbuscore/discussions](https://github.com/lgili/modbuscore/discussions)mbc_mbap_encode(&header, pdu, pdu_len, buffer, buf_size, &out_len);

- **Modbus Spec**: [modbus.org](https://modbus.org/specs.php)mbc_mbap_decode(buffer, length, &header, &pdu, &pdu_len);

mbc_mbap_expected_length(partial_buffer, partial_len);

## 🙏 Acknowledgments

// Transport Layer

- **libmodbus**: The reference Modbus implementationmbc_posix_tcp_create(&config, &transport, &ctx);

- **Community**: Thank you to all contributors and testers!mbc_transport_send(&transport, buffer, length, &io);

mbc_transport_receive(&transport, buffer, capacity, &io);

---```



<div align="center">**[📖 Full API Reference](docs/api/)**



**Made with ❤️ for the embedded & industrial automation community**---



⭐ **Star us on GitHub!** ⭐## 🔧 Build Options



</div>```bash

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

ModbusCore v1.0 is built with inspiration from:

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
