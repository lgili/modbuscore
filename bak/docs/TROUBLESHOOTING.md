# Troubleshooting Guide

**Goal:** Resolve the most common issues in under 5 minutes.

This guide covers the **top 10 errors** users encounter, ranked by frequency.

---

## 🔥 Top 10 Errors & Solutions

### 1. ❌ "Connection refused" / "No route to host" (TCP)

**Symptom:**
```
mb_host_tcp_connect() returns NULL
errno: 111 (Connection refused)
```

**Causes:**
- Modbus TCP server is not running
- Wrong IP address or port
- Firewall blocking port 502

**Solutions:**
```bash
# Check if server is reachable
ping 192.168.1.10

# Check if port 502 is open
telnet 192.168.1.10 502
# or
nc -zv 192.168.1.10 502

# Test with a known-good client
python3 -m pymodbus.console tcp --host 192.168.1.10 --port 502

# Allow port 502 through firewall (Linux)
sudo ufw allow 502/tcp
```

**Quick Fix:**
Start a test server:
```bash
python3 -m pymodbus.console tcp --port 502
```

---

### 2. ❌ Timeout / No Response (RTU)

**Symptom:**
```
mb_host_read_holding() returns MB_ERR_TIMEOUT
No data received from serial device
```

**Causes:**
- Wrong serial device path
- Wrong baud rate
- Device not responding (check wiring)
- T3.5 timeout too short

**Solutions:**
```bash
# List available serial ports
ls -l /dev/tty*        # Linux/macOS
mode                   # Windows

# Test serial port with echo
stty -F /dev/ttyUSB0 115200  # Set baud rate
cat /dev/ttyUSB0 &           # Read in background
echo "test" > /dev/ttyUSB0   # Send test data

# Check permissions (Linux)
sudo chmod 666 /dev/ttyUSB0
# or add user to dialout group
sudo usermod -a -G dialout $USER
```

**Quick Fix:**
Try a slower baud rate (9600) and increase timeout:
```c
mb_host_client_t *client = mb_host_rtu_connect("/dev/ttyUSB0", 9600);
mb_host_set_timeout(client, 5000); // 5 seconds
```

---

### 3. ❌ "Illegal data address" Exception (0x02)

**Symptom:**
```
mb_host_read_holding() returns MB_ERR_EXCEPTION
mb_host_last_exception() returns 0x02
```

**Cause:**
The server doesn't have a register at the requested address.

**Solutions:**
- Check the device manual for valid register addresses
- Verify the register type (holding vs input)
- Ensure the address is in the mapped range

**Example:**
```c
// Wrong: Server only has registers 0-99
mb_host_read_holding(client, 1, 1000, 10, regs); // ❌ Exception 0x02

// Correct: Read from valid range
mb_host_read_holding(client, 1, 0, 10, regs);    // ✅ OK
```

**Quick Fix:**
Start with address 0 and read 1 register to test connectivity:
```c
uint16_t test;
mb_err_t err = mb_host_read_holding(client, 1, 0, 1, &test);
```

---

### 4. ❌ CRC Error (RTU)

**Symptom:**
```
mb_rtu_validate_frame() returns MB_ERR_CRC
Received frame has bad CRC
```

**Causes:**
- Electrical noise on RS-485 line
- Wrong baud rate or parity settings
- Cable too long (>1200m at 9600 baud)
- Missing termination resistors

**Solutions:**
- Verify baud rate matches device (8N1 is most common)
- Add 120Ω termination resistors at both ends of RS-485 bus
- Use shielded cable
- Reduce baud rate for long cables
- Check for ground loops

**Quick Fix:**
```c
// Try with parity enabled (8E1 or 8O1)
// Use full API instead of mb_host for parity control
mb_rtu_transport_t rtu;
mb_rtu_transport_init(&rtu, port_impl, MB_PARITY_EVEN);
```

---

### 5. ❌ "Invalid function code" Exception (0x01)

**Symptom:**
```
mb_host_write_multiple_registers() returns MB_ERR_EXCEPTION
mb_host_last_exception() returns 0x01
```

**Cause:**
The server doesn't support the requested function code.

**Solutions:**
- Check device manual for supported function codes
- Use single-register writes (FC 0x06) instead of multiple (FC 0x10)
- Verify the device is a Modbus server (not just a serial device)

**Example:**
```c
// If device doesn't support FC 0x10 (Write Multiple Registers)
// Use FC 0x06 (Write Single Register) in a loop instead
for (int i = 0; i < count; i++) {
    mb_host_write_single_register(client, unit_id, address + i, values[i]);
}
```

---

### 6. ❌ Build Error: "modbus/mb_host.h: No such file or directory"

**Symptom:**
```bash
gcc hello.c -lmodbus
hello.c:1:10: fatal error: modbus/mb_host.h: No such file or directory
```

**Cause:**
Library not installed or not in include path.

**Solutions:**

**Option 1: Install system-wide**
```bash
cd build
cmake --build . --target install
# or
sudo make install
```

**Option 2: Use explicit include path**
```bash
gcc hello.c -I/path/to/modbus/include -L/path/to/modbus/lib -lmodbus
```

**Option 3: Use CMake (recommended)**
```cmake
find_package(ModbusCore REQUIRED)
target_link_libraries(your_app Modbus::modbus)
```

---

### 7. ❌ Segmentation Fault / Access Violation

**Symptom:**
```
Segmentation fault (core dumped)
```

**Common Causes:**

**a) Buffer too small:**
```c
// Wrong: Buffer only holds 5 registers, but reading 10
uint16_t regs[5];
mb_host_read_holding(client, 1, 0, 10, regs); // ❌ Buffer overflow
```
**Fix:** Match buffer size to count:
```c
uint16_t regs[10];
mb_host_read_holding(client, 1, 0, 10, regs); // ✅ OK
```

**b) NULL pointer:**
```c
// Wrong: Didn't check if connection succeeded
mb_host_client_t *client = mb_host_tcp_connect("invalid:999");
mb_host_read_holding(client, 1, 0, 1, &val); // ❌ client is NULL
```
**Fix:** Always check NULL:
```c
mb_host_client_t *client = mb_host_tcp_connect("192.168.1.10:502");
if (client == NULL) {
    fprintf(stderr, "Connection failed\n");
    return 1;
}
```

**c) Use-after-free:**
```c
// Wrong: Using client after disconnect
mb_host_disconnect(client);
mb_host_read_holding(client, 1, 0, 1, &val); // ❌ client freed
```

---

### 8. ❌ "Illegal data value" Exception (0x03)

**Symptom:**
```
mb_host_write_single_register() returns MB_ERR_EXCEPTION
mb_host_last_exception() returns 0x03
```

**Causes:**
- Register is read-only
- Value out of valid range
- Data format mismatch (e.g., writing to a 32-bit register as 16-bit)

**Solutions:**
- Check if register is writable (holding vs input registers)
- Verify value range in device manual
- Use correct data type (uint16, uint32, float)

**Example:**
```c
// Wrong: Writing to input registers (read-only)
mb_host_write_single_register(client, 1, 30001, 100); // ❌ Exception 0x03

// Correct: Write to holding registers
mb_host_write_single_register(client, 1, 40001, 100); // ✅ OK
```

---

### 9. ❌ Link Error: "undefined reference to `mb_host_tcp_connect`"

**Symptom:**
```bash
gcc hello.c -lmodbus
/usr/bin/ld: /tmp/ccXXX.o: undefined reference to `mb_host_tcp_connect`
```

**Cause:**
`mb_host.h` API not implemented yet (coming in Gate 20.5).

**Current Workaround:**
Use the full API instead:
```c
#include <modbus/mb_client.h>
#include <modbus/mb_tcp_transport.h>
#include <modbus/ports/modbus_port_posix.h>

// See examples/tcp_client_cli.c for full example
```

**Note:** The `mb_host.h` implementation is in progress. Use `examples/level1_hello/` as a template once available.

---

### 10. ❌ Wrong Data / Byte Order Issues

**Symptom:**
```
Expected: 1234
Got:      54018  (byte-swapped)
```

**Cause:**
Modbus uses big-endian (network byte order), but your system might expect little-endian.

**Solutions:**

**For 16-bit registers:** No swapping needed (handled by library)

**For 32-bit integers:** Use the library helpers:
```c
#include <modbus/utils.h>

uint16_t regs[2];
mb_host_read_holding(client, 1, 0, 2, regs);

// Extract 32-bit value with correct byte order
uint32_t value = modbus_get_uint32_from_uint16(regs, MODBUS_BIG_ENDIAN);
// or MODBUS_LITTLE_ENDIAN, MODBUS_LITTLE_ENDIAN_SWAP, etc.
```

**For floats:**
```c
float value = modbus_get_float_from_uint16(regs, MODBUS_BIG_ENDIAN);
```

---

## 🔧 Debugging Tips

### Enable Verbose Logging

```c
mb_host_enable_logging(client, true);
```

This prints all Modbus frames to stdout:
```
TX: [01 03 00 00 00 0A C5 CD]
RX: [01 03 14 00 00 00 01 ...]
```

### Use a Modbus Analyzer Tool

- **Desktop:** [QModMaster](https://sourceforge.net/projects/qmodmaster/)
- **Command-line:** `modpoll` or `mbtget`
- **Python:** `pymodbus.console`

### Check Hardware with Logic Analyzer

For RTU, use a USB logic analyzer (e.g., Saleae) to verify:
- TX/RX signals are correct
- Baud rate matches expectations
- Voltage levels are correct (RS-485: differential ±2V to ±6V)

---

## 📚 Still Stuck?

1. **Check examples:** `examples/level1_hello/` and `examples/`
2. **Read API docs:** https://lgili.github.io/modbus/docs/
3. **Search issues:** https://github.com/lgili/modbus/issues
4. **Ask for help:** Open a new issue with:
   - Platform (Linux/Windows/ESP32/etc.)
   - Transport (TCP/RTU)
   - Minimal code example
   - Error message / behavior
   - What you've tried so far

---

## 🎯 Quick Diagnosis Tree

```
Problem?
│
├─ Can't connect
│   ├─ TCP → Check server running, firewall, IP address
│   └─ RTU → Check device path, permissions, baud rate
│
├─ Timeout / no response
│   ├─ TCP → Check network connectivity (ping)
│   └─ RTU → Check wiring, baud rate, device powered on
│
├─ Exception code
│   ├─ 0x01 (Illegal function) → Device doesn't support this FC
│   ├─ 0x02 (Illegal address) → Register address out of range
│   ├─ 0x03 (Illegal value) → Value out of range or read-only
│   └─ 0x04 (Server failure) → Device internal error
│
├─ Wrong data
│   └─ Check byte order (use modbus_get_uint32_from_uint16)
│
├─ Build errors
│   └─ Install library or add -I/-L flags
│
└─ Crash / segfault
    └─ Check buffer size, NULL pointers, use-after-free
```

---

**Pro Tip:** Start with the simplest possible test (read 1 register from address 0) to verify basic connectivity before debugging complex operations.
