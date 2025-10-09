---
layout: default
title: ModbusCore Documentation
---

# ModbusCore Documentation

> **The Core of Modern Modbus Communication**

Welcome to the ModbusCore documentation. This library provides a modern, production-ready Modbus implementation for embedded systems and desktop applications, with configurable profiles from 8KB to unlimited.

## 📚 User Guides

### Core Features

- **[Zero-Copy IO (Gate 21)](zero_copy_io.md)** - Scatter-gather iovec for minimal memory usage
- **[Lock-Free Queues & Transaction Pool (Gate 22)](queue_and_pool.md)** - Deterministic memory management without malloc
- **[ISR-Safe Mode (Gate 23)](isr_safe_mode.md)** - Ultra-low latency for interrupt-driven embedded systems
- **[QoS & Backpressure (Gate 24)](qos_backpressure.md)** - Priority-aware queue management to prevent head-of-line blocking
- **[Compact Diagnostics (Gate 25)](diagnostics.md)** - Lightweight on-device debugging with minimal overhead

### Additional Documentation

- **[Troubleshooting Guide](TROUBLESHOOTING.md)** - Common issues and solutions

## 🔧 API Documentation

The project documentation is published via the [`docs` CMake preset](../README.md#documentation).

- Latest rendered API docs: once CI succeeds, browse the generated site on the `gh-pages` branch.
- To build locally, run `make docs` or `cmake --build --preset docs`.

## 🚀 Quick Links

- [Main Repository](https://github.com/lgili/modbus)
- [README](../README.md)
- [Examples](../examples/)
- [Embedded Quickstarts](../embedded/quickstarts/)
