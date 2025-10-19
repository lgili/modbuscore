---
layout: default
title: Guides
nav_order: 5
has_children: true
---

# Guides
{: .no_toc }

Practical guides for using ModbusCore v1.0
{: .fs-6 .fw-300 }

---

## Available Guides

### [Testing Guide](testing.md)
Learn how to run tests, write your own tests, and verify your Modbus implementation.

**Topics:**
- Running unit tests with CTest
- Integration testing with example servers
- Writing custom tests
- Using mock transport for testing

---

### [Porting Guide](porting.md)
Add custom transport layers for new platforms or protocols.

**Topics:**
- Transport interface specification
- Implementing custom transports
- Example: Adding UART/RTU support
- Testing your transport implementation

---

### [Troubleshooting](troubleshooting.md)
Common issues and solutions.

**Topics:**
- Connection problems
- Timeout issues
- Protocol errors
- Build and link errors
- Debugging techniques

---

## Quick Links

- **[Quick Start](../quick-start.md)** – Get started in 5 minutes
- **[Architecture](../architecture.md)** – Understand the design
- **[API Reference](../api/)** – Function documentation
- **[Examples](https://github.com/lgili/modbuscore/tree/main/tests)** – Working code samples
