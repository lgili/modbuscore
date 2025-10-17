# Level 1: Hello World Examples

**Goal:** Get started with Modbus in under 2 minutes.

These examples use the simplified `mb_host.h` API, which handles all setup automatically. Perfect for learning and prototyping.

---

## 🚀 Quick Start

### TCP Client (reads 1 register)

```c
#include <modbus/mb_host.h>
#include <stdio.h>

int main(void) {
    mb_host_client_t *client = mb_host_tcp_connect("127.0.0.1:502");
    if (!client) { return 1; }

    uint16_t value;
    mb_err_t err = mb_host_read_holding(client, 1, 0, 1, &value);
    printf("Register 0: %u (%s)\n", value, mb_host_error_string(err));

    mb_host_disconnect(client);
    return (err == MB_OK) ? 0 : 1;
}
```

**Build & Run:**
```bash
gcc hello_tcp.c -lmodbus -o hello_tcp
./hello_tcp
```

---

## 📁 Files

| File | Description | Lines |
|------|-------------|-------|
| `hello_tcp.c` | Simplest TCP client - reads 1 register | 10 |
| `hello_rtu.c` | Simplest RTU client - serial port | 10 |
| `hello_write.c` | Write a single register | 12 |

---

## ⚠️ Prerequisites

1. **TCP:** You need a Modbus TCP server running on `127.0.0.1:502`
   - Quick test server: `python3 -m pymodbus.console tcp --port 502`
   - Or use a Modbus simulator like ModSim

2. **RTU:** You need a serial device at `/dev/ttyUSB0` (Linux) or `COM3` (Windows)
   - Adjust the device path in `hello_rtu.c`

---

## 🎯 Next Steps

- **Want more control?** See `examples/level2_basic/` for full API usage
- **Need a server?** See `examples/tcp_server_demo.c`
- **Embedded target?** See `embedded/quickstarts/` for platform-specific setup
- **Error handling?** Read `docs/TROUBLESHOOTING.md`

---

## 📝 Notes

- The `mb_host.h` API is designed for **desktop prototyping** (Linux, macOS, Windows)
- For **embedded systems**, use the full `mb_client_*` API with custom transports
- For **production**, add proper error handling, timeouts, and reconnection logic
