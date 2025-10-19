---
layout: default
title: Troubleshooting
parent: Guides
nav_order: 3
---

# Troubleshooting Guide
{: .no_toc }

Common issues and solutions for ModbusCore v1.0
{: .fs-6 .fw-300 }

## Table of Contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Connection Issues

### Failed to Connect (MBC_STATUS_IO_ERROR)

**Symptoms:**
```
✗ Failed to connect to 192.168.1.100:502 (status=-5)
```

**Common Causes:**

1. **Server not running**
   ```bash
   # Check if server is listening
   netstat -an | grep 502
   # Or on macOS/Linux:
   lsof -i :502
   ```

2. **Wrong IP address or port**
   ```c
   // Double-check configuration
   mbc_posix_tcp_config_t config = {
       .host = "192.168.1.100",  // Verify this IP!
       .port = 502,               // Standard Modbus TCP port
       .connect_timeout_ms = 5000,
   };
   ```

3. **Firewall blocking connection**
   ```bash
   # Linux: check firewall
   sudo iptables -L -n | grep 502

   # macOS: check firewall
   sudo /usr/libexec/ApplicationFirewall/socketfilterfw --getglobalstate

   # Windows: check Windows Firewall
   # Control Panel → Windows Defender Firewall → Advanced Settings
   ```

4. **Network unreachable**
   ```bash
   # Test network connectivity
   ping 192.168.1.100

   # Test TCP connectivity
   telnet 192.168.1.100 502
   # Or:
   nc -zv 192.168.1.100 502
   ```

**Solutions:**

✅ **Start a test server:**
```bash
# Python test server (included)
cd tests
python3 simple_tcp_server.py
# Listens on 127.0.0.1:5502
```

✅ **Update connection config:**
```c
mbc_posix_tcp_config_t config = {
    .host = "127.0.0.1",  // Localhost for testing
    .port = 5502,         // Test server port
    .connect_timeout_ms = 10000,  // Increase timeout
    .recv_timeout_ms = 2000,
};
```

✅ **Check Docker/VM networking:**
```bash
# If server is in Docker container, expose port:
docker run -p 502:502 your-modbus-server
```

---

## Timeout Issues

### Response Timeout (MBC_STATUS_TIMEOUT)

**Symptoms:**
```
✗ Timeout waiting for response (iteration 99)
```

**Common Causes:**

1. **Server too slow to respond**
2. **Network latency**
3. **Wrong unit ID**
4. **Server busy or hung**

**Solutions:**

✅ **Increase timeout:**
```c
mbc_engine_config_t engine_config = {
    .runtime = &runtime,
    .role = MBC_ENGINE_ROLE_CLIENT,
    .framing = MBC_FRAMING_TCP,
    .response_timeout_ms = 5000,  // Increase from 3000 to 5000
};
```

✅ **Verify unit ID:**
```c
// Make sure unit ID matches server configuration
mbc_pdu_build_read_holding_request(&pdu,
    1,      // Unit ID - typically 1 for TCP
    0,      // Address
    10      // Quantity
);
```

✅ **Test with minimal request:**
```c
// Try reading just 1 register
mbc_pdu_build_read_holding_request(&pdu, 1, 0, 1);
```

✅ **Check server status:**
```bash
# Verify server is responding
echo -ne '\x00\x01\x00\x00\x00\x06\x01\x03\x00\x00\x00\x01' | nc 192.168.1.100 502 | xxd
```

---

## Data/Protocol Issues

### Exception Response (0x02 - ILLEGAL DATA ADDRESS)

**Symptoms:**
```
✗ Server returned exception: FC=0x03, Code=0x02
```

**Cause:** Register address out of range for server.

**Solutions:**

✅ **Check server's address range:**
```c
// Different servers have different ranges
// Typical ranges:
// - PLCs: 40001-49999 (Modbus addresses 0-9999)
// - Simulators: Often 0-9999 or 0-99

// Try address 0 first:
mbc_pdu_build_read_holding_request(&pdu, 1, 0, 1);
```

✅ **Reduce quantity:**
```c
// Server may not have 100 consecutive registers
mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);  // Instead of 100
```

✅ **Consult server documentation** for valid address ranges.

---

### Exception Response (0x03 - ILLEGAL DATA VALUE)

**Symptoms:**
```
✗ Server returned exception: FC=0x03, Code=0x03
```

**Causes:**
- Quantity out of range (0 or >125 for FC03)
- Invalid register values for write operations

**Solutions:**

✅ **Validate quantity:**
```c
// FC03: quantity must be 1-125
if (quantity < 1 || quantity > 125) {
    fprintf(stderr, "Invalid quantity: %u\n", quantity);
    return 1;
}

mbc_pdu_build_read_holding_request(&pdu, 1, 0, quantity);
```

✅ **Check write values:**
```c
// Some registers may have restricted value ranges
// Example: register 100 might only accept 0-1000
uint16_t value = 500;  // Valid
mbc_pdu_build_write_single_register(&pdu, 1, 100, value);
```

---

### MBAP Decoding Error

**Symptoms:**
```
MBC_STATUS_DECODING_ERROR when calling mbc_mbap_decode()
```

**Causes:**
- Protocol ID != 0
- Length field mismatch
- Corrupted data

**Solutions:**

✅ **Dump received data:**
```c
printf("Received %zu bytes:\n", rx_length);
for (size_t i = 0; i < rx_length; i++) {
    printf("%02X ", rx_buffer[i]);
    if ((i + 1) % 16 == 0) printf("\n");
}
printf("\n");
```

✅ **Verify MBAP header:**
```c
if (rx_length < 7) {
    fprintf(stderr, "Incomplete MBAP header (need 7 bytes, got %zu)\n", rx_length);
    return;
}

uint16_t protocol_id = (rx_buffer[2] << 8) | rx_buffer[3];
if (protocol_id != 0) {
    fprintf(stderr, "Invalid protocol ID: 0x%04X (expected 0x0000)\n", protocol_id);
    return;
}

uint16_t length = (rx_buffer[4] << 8) | rx_buffer[5];
printf("MBAP length field: %u\n", length);
```

✅ **Use Wireshark to inspect traffic:**
```bash
# Capture Modbus TCP traffic
sudo tcpdump -i any -w modbus.pcap port 502

# View in Wireshark with Modbus dissector
wireshark modbus.pcap
```

---

## Build Issues

### Header Not Found

**Symptoms:**
```
fatal error: modbuscore/transport/posix_tcp.h: No such file or directory
```

**Solutions:**

✅ **Add include directory:**
```bash
gcc your_app.c \
    -I/path/to/modbuscore/include \
    -L/path/to/modbuscore/build \
    -lmodbuscore \
    -o your_app
```

✅ **CMake: link properly:**
```cmake
target_link_libraries(your_app PRIVATE modbuscore)
# This automatically adds include directories
```

---

### Undefined Reference Errors

**Symptoms:**
```
undefined reference to `mbc_posix_tcp_create'
```

**Solutions:**

✅ **Link library:**
```bash
gcc your_app.c -lmodbuscore -o your_app
```

✅ **Check library path:**
```bash
# Linux/macOS:
export LD_LIBRARY_PATH=/path/to/modbuscore/build:$LD_LIBRARY_PATH

# Or use -L flag:
gcc your_app.c -L/path/to/modbuscore/build -lmodbuscore -o your_app
```

✅ **Verify library was built:**
```bash
ls -la /path/to/modbuscore/build/libmodbuscore.a
```

---

## Runtime Issues

### Engine Returns MBC_STATUS_BUSY

**Symptoms:**
```c
status = mbc_engine_submit_request(&engine, req2, len2);
// Returns MBC_STATUS_BUSY
```

**Cause:** Engine is already processing a request (state = WAIT_RESPONSE).

**Solution:**

✅ **Wait for current request to complete:**
```c
// Submit first request
mbc_engine_submit_request(&engine, req1, len1);

// Poll until response or timeout
bool received = false;
for (int i = 0; i < 100; ++i) {
    mbc_engine_step(&engine, 10);

    mbc_pdu_t response;
    if (mbc_engine_take_pdu(&engine, &response)) {
        // Process response...
        received = true;
        break;
    }
}

if (received) {
    // Now safe to submit next request
    mbc_engine_submit_request(&engine, req2, len2);
}
```

---

### Buffer Overflow (MBC_STATUS_NO_RESOURCES)

**Symptoms:**
```
MBC_STATUS_NO_RESOURCES from mbc_engine_step()
```

**Cause:** RX buffer full (260 bytes max for TCP).

**Solution:**

✅ **Process PDUs before receiving more:**
```c
while (true) {
    mbc_engine_step(&engine, 10);

    // Take and process PDU immediately
    mbc_pdu_t pdu;
    if (mbc_engine_take_pdu(&engine, &pdu)) {
        // Process PDU here - this frees buffer space
        process_pdu(&pdu);
    }
}
```

---

## Performance Issues

### Slow Response Times

**Symptoms:**
- Takes many iterations to receive response
- High latency

**Solutions:**

✅ **Increase step budget:**
```c
// Process more bytes per iteration
mbc_engine_step(&engine, 100);  // Instead of 10
```

✅ **Reduce polling delay:**
```c
// Poll more frequently
for (int i = 0; i < 1000; ++i) {
    mbc_engine_step(&engine, 10);
    // No sleep/delay here
}
```

✅ **Use blocking mode (custom transport):**
```c
// Implement custom transport with blocking receive
// (Advanced - see porting guide)
```

---

## Debugging Tips

### Enable Verbose Logging

```c
void debug_mbap_frame(const uint8_t *buffer, size_t length) {
    printf("MBAP Frame (%zu bytes):\n", length);
    printf("  Transaction ID: %u\n", (buffer[0] << 8) | buffer[1]);
    printf("  Protocol ID: %u\n", (buffer[2] << 8) | buffer[3]);
    printf("  Length: %u\n", (buffer[4] << 8) | buffer[5]);
    printf("  Unit ID: %u\n", buffer[6]);
    printf("  PDU: ");
    for (size_t i = 7; i < length; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\n");
}
```

### Use Event Callbacks

```c
void engine_event_cb(void *ctx, const mbc_engine_event_t *event) {
    switch (event->type) {
        case MBC_ENGINE_EVENT_STATE_CHANGE:
            printf("[%llu] State change\n", event->timestamp_ms);
            break;
        case MBC_ENGINE_EVENT_RX_READY:
            printf("[%llu] RX ready\n", event->timestamp_ms);
            break;
        case MBC_ENGINE_EVENT_TX_SENT:
            printf("[%llu] TX sent\n", event->timestamp_ms);
            break;
        case MBC_ENGINE_EVENT_TIMEOUT:
            printf("[%llu] Timeout!\n", event->timestamp_ms);
            break;
        default:
            break;
    }
}

mbc_engine_config_t engine_config = {
    // ...
    .event_cb = engine_event_cb,
    .event_ctx = NULL,
};
```

### Packet Capture

```bash
# Capture Modbus TCP traffic
sudo tcpdump -i any -A -s 0 'tcp port 502'

# Save to file for Wireshark
sudo tcpdump -i any -w modbus.pcap 'tcp port 502'

# View with Wireshark's Modbus dissector
wireshark modbus.pcap
```

---

## Getting Help

If you're still stuck:

1. **Check Examples**: [`tests/example_tcp_client_fc03.c`](https://github.com/lgili/modbuscore/blob/main/tests/example_tcp_client_fc03.c)

2. **Search Issues**: [GitHub Issues](https://github.com/lgili/modbuscore/issues)

3. **Ask for Help**: [GitHub Discussions](https://github.com/lgili/modbuscore/discussions)

4. **Read API Docs**: [API Reference](../api/)

When reporting issues, please include:
- ModbusCore version
- Operating system
- Server type/model
- Complete error message
- Minimal reproducible example
- Packet capture (if possible)
