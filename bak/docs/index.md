---
layout: default
title: ModbusCore Documentation
---

# ModbusCore Documentation

> **The Core of Modern Modbus Communication**

Welcome to the ModbusCore documentation. This library provides a modern, production-ready Modbus implementation for embedded systems and desktop applications, with configurable profiles from 8KB to unlimited.

## 🚀 Quick Start & Setup

- **[Try It Now](TRY_IT_NOW.md)** – Build the loopback demo and compatibility shim in minutes.
- **[Installation Guide](INSTALLATION_GUIDE.md)** – System installs, custom prefixes, and amalgamated builds.
- **[CMake Presets Guide](CMAKE_PRESETS_GUIDE.md)** – Configure native, compat, and embedded builds with one command.
- **[Kconfig & Menuconfig](KCONFIG_GUIDE.md)** – Surface ModbusCore options inside Zephyr and ESP-IDF.
- **CMake/pkg-config Consumers** – See `examples/cmake-consume/` and `examples/pkgconfig-consume/` after installation.

## 🎯 Feature Deep Dives

- **[Zero-Copy IO (Gate 21)](zero_copy_io.md)** – Scatter/gather strategies for minimal copying.
- **[Lock-Free Queues & Transaction Pool (Gate 22)](queue_and_pool.md)** – Deterministic memory management without malloc.
- **[ISR-Safe Mode (Gate 23)](isr_safe_mode.md)** – Ultra-low latency for interrupt-driven embedded systems.
- **[QoS & Backpressure (Gate 24)](qos_backpressure.md)** – Priority-aware queue management to prevent head-of-line blocking.
- **[Compact Diagnostics (Gate 25)](diagnostics.md)** – Lightweight on-device debugging with minimal overhead.
- **[Power Management](power_management.md)** – Idle detection, callbacks, and low-power integration tips.

## 🛡️ Reliability & Support

- **[Noisy Link Playbook](noisy_links_playbook.md)** – Acceptance scenarios for Gate 33 fault-injection and resync.
- **[Troubleshooting Guide](TROUBLESHOOTING.md)** – Common issues and how to resolve them quickly.

## 📏 Planning & Metrics

- **[Footprint Report](FOOTPRINT.md)** – ROM/RAM snapshots across profiles and transports.
- **[Stack Analysis](STACK_ANALYSIS.md)** – Worst-case stack usage by execution path.
- **[Resource Planning](RESOURCE_PLANNING.md)** – Sizing guidance for popular MCU families.

## 🔄 Migration & Release

- **[Porting from libmodbus](PORTING_LIBMODBUS.md)** – Leverage the Gate 34 compatibility shim.
- **[Release Checklist](RELEASE_CHECKLIST.md)** – Repeatable playbook for tagging and publishing new versions.

## 🔧 API Documentation

The project documentation is published via the [`docs` CMake preset](../README.md#documentation).

- Latest rendered API docs: once CI succeeds, browse the generated site on the `gh-pages` branch.
- To build locally, run `make docs` or `cmake --build --preset docs`.

## 🚀 Quick Links

- [Main Repository](https://github.com/lgili/modbus)
- [README](../README.md)
- [Examples](../examples/)
- [Embedded Quickstarts](../embedded/quickstarts/)
