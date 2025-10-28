# ModbusCore Documentation

Welcome to ModbusCore! This guide will help you get started quickly.

## 📚 Documentation Index

### Getting Started
- **[Quick Start](getting-started/quick-start.md)** - Get running in 5 minutes
- **[Installation](getting-started/installation.md)** - Install ModbusCore on your system
- **[First Application](getting-started/first-application.md)** - Build your first Modbus app
- **[Examples](getting-started/examples.md)** - Ready-to-run examples

### User Guide
- **[Architecture Overview](user-guide/architecture.md)** - Understanding ModbusCore design
- **[Transports](user-guide/transports.md)** - TCP, RTU, and custom transports
- **[Protocol Engine](user-guide/protocol-engine.md)** - Client and server modes
- **[Runtime & Dependencies](user-guide/runtime.md)** - Dependency injection system
- **[Auto-Heal & Resilience](user-guide/auto-heal.md)** - Automatic retry and recovery
- **[Diagnostics](user-guide/diagnostics.md)** - Logging and telemetry
- **[Testing & Mocking](user-guide/testing.md)** - Unit testing your code

### Advanced Topics
- **[Custom Transports](advanced/custom-transports.md)** - Implement your own transport
- **[Embedded Systems](advanced/embedded.md)** - FreeRTOS, Zephyr, bare metal
- **[Performance Tuning](advanced/performance.md)** - Optimize for your use case
- **[Security](advanced/security.md)** - Best practices
- **[Fuzzing](advanced/fuzzing.md)** - Continuous fuzzing setup

### API Reference
- **[Common Types](api/common.md)** - Status codes, errors
- **[Protocol](api/protocol.md)** - PDU, MBAP, CRC, Engine
- **[Runtime](api/runtime.md)** - Runtime, Builder, Dependencies
- **[Transport](api/transport.md)** - Interface and implementations
- **[Auto-Heal](api/auto-heal.md)** - Supervisor API

### Contributing
- **[Development Setup](contributing/development.md)** - Set up development environment
- **[Code Style](contributing/code-style.md)** - Coding standards
- **[Testing Guidelines](contributing/testing.md)** - Writing tests
- **[Pull Request Process](contributing/pull-requests.md)** - How to contribute

### Reference
- **[Changelog](../CHANGELOG.md)** - Version history
- **[Versioning Policy](../VERSIONING.md)** - SemVer and LTS
- **[Roadmap](roadmap.md)** - Future plans
- **[Troubleshooting](reference/troubleshooting.md)** - Common issues
- **[FAQ](reference/faq.md)** - Frequently asked questions
- **[Glossary](reference/glossary.md)** - Terms and definitions

## 🚀 Quick Navigation

### I want to...

**...get started quickly**
→ [Quick Start](getting-started/quick-start.md)

**...install ModbusCore**
→ [Installation Guide](getting-started/installation.md)

**...build a TCP client**
→ [TCP Client Example](getting-started/examples.md#tcp-client)

**...build an RTU server**
→ [RTU Server Example](getting-started/examples.md#rtu-server)

**...understand the architecture**
→ [Architecture Overview](user-guide/architecture.md)

**...implement a custom transport**
→ [Custom Transports](advanced/custom-transports.md)

**...use on embedded systems**
→ [Embedded Systems Guide](advanced/embedded.md)

**...contribute to the project**
→ [Contributing Guide](contributing/development.md)

**...report a bug**
→ [GitHub Issues](https://github.com/lgili/modbuscore/issues)

**...get help**
→ [Support](../SUPPORT.md)

## 📖 Documentation Conventions

Throughout this documentation:

- **Code blocks** show runnable examples
- **💡 Tips** highlight best practices
- **⚠️ Warnings** indicate potential pitfalls
- **📝 Notes** provide additional context

## 🔗 External Resources

- [Modbus Specification](https://modbus.org/specs.php)
- [CMake Documentation](https://cmake.org/documentation/)
- [C17 Standard](https://www.iso.org/standard/74528.html)

---

**Next**: [Quick Start →](getting-started/quick-start.md)
