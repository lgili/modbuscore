# Changelog

All notable changes to ModbusCore will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial work on Phase 8: Distribution and packaging infrastructure

## [1.0.0] - TBD

### Added

#### Core Architecture
- **Dependency Injection Runtime**: Complete DI container with builders for runtime configuration
- **Pluggable Interfaces**: Clean abstraction for transport, clock, allocator, logger, and diagnostics
- **Protocol Engine**: Stateless Modbus protocol engine with event-driven architecture
- **Client & Server**: Full client and server state machines with configurable timeouts

#### Transport Layer
- **POSIX TCP**: Non-blocking TCP transport for Linux/macOS/BSD
- **Windows TCP**: Winsock-based TCP transport for Windows
- **POSIX RTU**: Serial/UART transport with termios (Linux/macOS)
- **Windows RTU**: Serial transport using Win32 API
- **Bare Metal RTU**: UART driver for embedded systems
- **Mock Transport**: Deterministic mock for testing with latency/failure injection

#### Protocol Support
- **Modbus TCP (MBAP)**: Full encoder/decoder with header validation
- **Modbus RTU**: CRC-16 computation and validation
- **PDU Builders**: Helper functions for FC03, FC06, FC16 (read/write registers)
- **Exception Handling**: Complete exception code support

#### Robustness & Reliability
- **Auto-heal Supervisor**: Automatic retry with exponential backoff and circuit breaker
- **Diagnostics System**: Structured event sink for telemetry and tracing
- **Fuzzing Infrastructure**: LibFuzzer harnesses for MBAP, RTU CRC, and PDU parsing
  - Seed corpus with 15 valid Modbus frames
  - CI/CD integration (PR: 60s, Weekly: 6h campaigns)
  - Docker-based fuzzing for macOS compatibility
- **Resilience Testing**: Mock-based tests for high latency, packet loss, and noise

#### Developer Experience
- **CMake Package**: Modern CMake with `find_package(ModbusCore)` support
- **pkg-config**: `.pc` file for Unix-like systems
- **Conan Package**: Recipe for Conan package manager
- **vcpkg Port**: Manifest and portfile for vcpkg integration
- **Examples**: POSIX TCP client, Windows TCP client, RTU demos, diagnostics showcase
- **Documentation**: Comprehensive API docs, architecture guides, troubleshooting

#### Testing
- **Unit Tests**: 100% coverage of core protocol logic
- **Integration Tests**: End-to-end TCP client/server tests
- **Smoke Tests**: Quick validation of all transport layers
- **Fuzzing**: Continuous fuzzing with LibFuzzer
- **CI/CD**: GitHub Actions workflows for Linux, macOS, Windows

### Changed
- N/A (initial release)

### Deprecated
- N/A (initial release)

### Removed
- Legacy 2.x API completely removed in favor of modern DI-based architecture

### Fixed
- N/A (initial release)

### Security
- **Fuzzing**: All parsers fuzzed with LibFuzzer to prevent memory corruption
- **Bounds Checking**: All buffer operations validated
- **Integer Overflow**: Safe arithmetic throughout
- **Input Validation**: All user inputs sanitized

---

## Release History Template

```markdown
## [X.Y.Z] - YYYY-MM-DD

### Added
- New features

### Changed
- Changes in existing functionality

### Deprecated
- Soon-to-be removed features

### Removed
- Removed features

### Fixed
- Bug fixes

### Security
- Security fixes
```

---

## Version Links

[Unreleased]: https://github.com/lgili/modbuscore/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/lgili/modbuscore/releases/tag/v1.0.0
