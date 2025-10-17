# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-10-10

### Added
- Gate 33 resilience features: fast RTU resync, duplicate filter, and fault-injection harness.
- Gate 34 compatibility layer mirroring libmodbus FC 03/06/16 APIs and migration guide.
- Loopback integration tests covering RTU/TCP queues, QoS, and noisy-link scenarios.
- Fresh documentation for noisy links, quick tryouts, power management, Kconfig/menuconfig, and installation presets.
- Examples for bare-metal, FreeRTOS, Zephyr, ESP-IDF, the host loopback demo, and libmodbus migration.

### Changed
- Consolidated client convenience request scaffolding and host helpers to reduce duplication.
- Hardened default configuration profiles (TINY/LEAN/FULL) and build presets for gates and footprint tracking.

### Fixed
- Resolved duplicate detection edge cases when retransmissions arrive immediately after queue insertion.
- Ensured compatibility shim mirrors libmodbus error semantics on non-POSIX platforms.

## [Unreleased]
- Pending entries will appear here once development resumes on the next minor release.

[1.0.0]: https://github.com/lgili/modbus/releases/tag/v1.0.0
