# Simple API Examples

Welcome to the **NEW** ModbusCore Simple API! These examples demonstrate how easy it is to use Modbus with the unified `mb_simple.h` interface.

## Quick Comparison

### Before (Old API - 50+ lines)
```c
#include <modbus/client.h>
#include <modbus/client_sync.h>
#include "../common/demo_tcp_socket.h"

demo_tcp_socket_t socket_ctx;
mb_transport_if_t *iface = demo_tcp_socket_connect(&socket_ctx, "192.168.1.10", 502, 2000);
mb_client_t client;
mb_client_txn_t txn_pool[4];
mb_client_init_tcp(&client, iface, txn_pool, 4);
uint16_t registers[10];
mb_client_read_holding_sync(&client, 1, 0, 10, registers, NULL);
demo_tcp_socket_close(&socket_ctx);
```

### After (New API - 3 lines!)
```c
#include <modbus/mb_simple.h>

mb_t *mb = mb_create_tcp("192.168.1.10:502");
mb_read_holding(mb, 1, 0, 10, registers);
mb_destroy(mb);
```

**80% less code!** 🎉

---

## Available Examples

### 1. hello_world.c - The Simplest Example
**3 lines of code** to read a Modbus register.

```bash
gcc -DMB_USE_PROFILE_SIMPLE hello_world.c -lmodbus -o hello_world
./hello_world 192.168.1.10:502
```

**Output:**
```
✓ Register 0: 1234
```

Perfect for getting started!

---

### 2. auto_cleanup.c - Automatic Resource Management
Shows how `MB_AUTO` automatically cleans up resources (GCC/Clang only).

```bash
gcc -DMB_USE_PROFILE_SIMPLE auto_cleanup.c -lmodbus -o auto_cleanup
./auto_cleanup
```

**Benefits:**
- No manual `mb_destroy()` calls
- No resource leaks even on early returns
- Exception-safe (RAII-style in C)

---

### 3. full_example.c - Complete Demo
Comprehensive example showing all features:
- Custom options (timeout, retries, logging)
- Reading (holding, input, coils, discrete)
- Writing (single, multiple)
- Error handling
- Convenience macros (`MB_CHECK`, `MB_LOG_ERROR`)

```bash
gcc -DMB_USE_PROFILE_SIMPLE full_example.c -lmodbus -o full_example
./full_example 192.168.1.10:502
```

---

## API Highlights

### Connection (TCP)
```c
/* Simple */
mb_t *mb = mb_create_tcp("192.168.1.10:502");

/* With options */
mb_options_t opts;
mb_options_init(&opts);
opts.timeout_ms = 5000;
opts.enable_logging = true;
mb_t *mb = mb_create_tcp_ex("192.168.1.10:502", &opts);
```

### Connection (RTU)
```c
mb_t *mb = mb_create_rtu("/dev/ttyUSB0", 115200);
```

### Reading
```c
uint16_t regs[10];
mb_read_holding(mb, 1, 0, 10, regs);  /* Holding registers */
mb_read_input(mb, 1, 0, 10, regs);     /* Input registers */

uint8_t coils[2];
mb_read_coils(mb, 1, 0, 16, coils);    /* Coils (16 coils = 2 bytes) */
```

### Writing
```c
mb_write_register(mb, 1, 100, 1234);           /* Single register */
mb_write_coil(mb, 1, 10, true);                /* Single coil */

uint16_t vals[] = {100, 200, 300};
mb_write_registers(mb, 1, 0, 3, vals);         /* Multiple registers */
```

### Error Handling
```c
mb_err_t err = mb_read_holding(mb, 1, 0, 10, regs);
if (err != MB_OK) {
    fprintf(stderr, "Error: %s\n", mb_error_string(err));
    if (err == MB_ERR_EXCEPTION) {
        uint8_t exc = mb_last_exception(mb);
        fprintf(stderr, "Exception code: 0x%02X\n", exc);
    }
}
```

### Convenience Macros
```c
/* Auto-cleanup (GCC/Clang) */
MB_AUTO(mb, mb_create_tcp("192.168.1.10:502"));

/* Error checking with early return */
MB_CHECK(mb_write_register(mb, 1, 100, 1234), "Write failed");

/* Error logging (doesn't return) */
MB_LOG_ERROR(mb_read_holding(mb, 1, 0, 10, regs), "Read failed");
```

---

## Building All Examples

```bash
# With GCC
gcc -DMB_USE_PROFILE_SIMPLE hello_world.c -lmodbus -o hello_world
gcc -DMB_USE_PROFILE_SIMPLE auto_cleanup.c -lmodbus -o auto_cleanup
gcc -DMB_USE_PROFILE_SIMPLE full_example.c -lmodbus -o full_example

# Or with CMake
cmake --preset profile-simple
cmake --build --preset profile-simple
```

---

## Comparison: Old vs New API

| Aspect | Old API | New API (mb_simple.h) | Improvement |
|--------|---------|----------------------|-------------|
| **Lines for "Hello World"** | ~50 | 3 | **-94%** |
| **Transport setup** | Manual | Automatic | **Much easier** |
| **Buffer management** | Manual | Automatic | **No mistakes** |
| **Resource cleanup** | Manual | Auto (MB_AUTO) | **No leaks** |
| **Error messages** | mb_err_str() | mb_error_string() | **Clearer** |
| **API consistency** | 3 layers | 1 unified | **Simpler** |

---

## Which API Should I Use?

### Use mb_simple.h if you want:
- ✅ Quick prototyping
- ✅ Desktop applications
- ✅ Simple use cases (80% of users)
- ✅ Minimal boilerplate
- ✅ Easy learning curve

### Use full API (client.h/server.h) if you need:
- 🔧 Async operations
- 🔧 Fine-grained control
- 🔧 Multiple concurrent connections
- 🔧 Custom transports
- 🔧 Advanced features (QoS, ISR mode, etc.)

**Both APIs work together!** You can start with `mb_simple.h` and migrate to the full API later if needed.

---

## Next Steps

1. **Start with hello_world.c** - Get familiar with the basics
2. **Try auto_cleanup.c** - Learn about automatic resource management
3. **Explore full_example.c** - See all features in action
4. **Read the docs** - See [docs/API_SIMPLIFIED.md](../../docs/API_SIMPLIFIED.md)
5. **Check profiles** - Configure for your platform: [docs/CONFIGURATION_PROFILES.md](../../docs/CONFIGURATION_PROFILES.md)

---

## FAQ

**Q: Does mb_simple.h replace the old API?**
A: No! The old API (client.h, server.h, mb_host.h) still works. mb_simple.h is an additional option for simpler use cases.

**Q: Can I use MB_AUTO on MSVC/Windows?**
A: MB_AUTO uses GCC/Clang cleanup attribute. On MSVC, you must call `mb_destroy()` manually.

**Q: Is mb_simple.h slower than the full API?**
A: No! It's a thin wrapper over the existing APIs with zero runtime overhead.

**Q: Can I mix mb_simple.h with the full API?**
A: Yes, but not recommended. Stick to one API style per project for clarity.

**Q: Where is mb_simple.h available?**
A: ModbusCore v2.0+ (January 2025)

---

**Made simple, kept powerful.** 🚀
