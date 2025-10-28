# Troubleshooting Guide

Solutions to common problems with ModbusCore.

## Table of Contents

- [Installation Issues](#installation-issues)
- [Build Errors](#build-errors)
- [Runtime Errors](#runtime-errors)
- [Communication Issues](#communication-issues)
- [Performance Problems](#performance-problems)
- [Platform-Specific](#platform-specific)

## Installation Issues

### CMake can't find ModbusCore

**Problem:**
```
CMake Error: Could not find a package configuration file provided by "ModbusCore"
```

**Solutions:**

1. **Set CMAKE_PREFIX_PATH:**
   ```bash
   cmake -B build -DCMAKE_PREFIX_PATH=/usr/local
   ```

2. **Check installation:**
   ```bash
   ls /usr/local/lib/cmake/ModbusCore/
   # Should contain ModbusCoreConfig.cmake
   ```

3. **Reinstall:**
   ```bash
   sudo cmake --install build --prefix /usr/local
   ```

---

### pkg-config not found

**Problem:**
```bash
$ pkg-config --modversion modbuscore
Package modbuscore was not found
```

**Solutions:**

1. **Set PKG_CONFIG_PATH:**
   ```bash
   export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
   ```

2. **Verify .pc file exists:**
   ```bash
   ls /usr/local/lib/pkgconfig/modbuscore.pc
   ```

3. **Add to shell profile:**
   ```bash
   echo 'export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH' >> ~/.bashrc
   source ~/.bashrc
   ```

---

### Permission denied during install

**Problem:**
```
CMake Error: Error in file installation
```

**Solutions:**

1. **Use sudo (Linux/macOS):**
   ```bash
   sudo cmake --install build
   ```

2. **Install to user directory:**
   ```bash
   cmake -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
   cmake --install build  # No sudo needed
   ```

3. **Add to PATH:**
   ```bash
   export CMAKE_PREFIX_PATH=$HOME/.local:$CMAKE_PREFIX_PATH
   ```

---

## Build Errors

### Compiler not found

**Problem:**
```
CMake Error: CMAKE_C_COMPILER not set
```

**Solutions:**

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install build-essential
```

**Linux (Fedora/RHEL):**
```bash
sudo dnf install gcc cmake
```

**macOS:**
```bash
xcode-select --install
```

**Windows:**
Install [Visual Studio 2019+](https://visualstudio.microsoft.com/) with C++ tools

---

### C17 not supported

**Problem:**
```
error: 'for' loop initial declarations are only allowed in C99 or C11 mode
```

**Solutions:**

1. **Update compiler:**
   ```bash
   gcc --version  # Should be 9.0+
   ```

2. **Force C17:**
   ```bash
   cmake -B build -DCMAKE_C_STANDARD=17
   ```

3. **Install modern compiler:**
   ```bash
   sudo apt-get install gcc-11
   export CC=gcc-11
   cmake -B build
   ```

---

### Undefined reference errors

**Problem:**
```
undefined reference to `mbc_pdu_build_read_holding_request'
```

**Solutions:**

1. **Link ModbusCore library:**
   ```cmake
   target_link_libraries(myapp ModbusCore::modbuscore)
   ```

2. **Check library exists:**
   ```bash
   ls /usr/local/lib/libmodbuscore.a
   ```

3. **Rebuild and reinstall:**
   ```bash
   cmake --build build --clean-first
   sudo cmake --install build
   ```

---

## Runtime Errors

### MBC_STATUS_INVALID_PARAMETER

**Problem:**
```c
mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 200);
// Returns MBC_STATUS_INVALID_PARAMETER
```

**Causes & Solutions:**

1. **Count too large:**
   ```c
   // ❌ Max 125 registers
   mbc_pdu_build_read_holding_request(&pdu, 1, 0, 200);
   
   // ✅ Within limits
   mbc_pdu_build_read_holding_request(&pdu, 1, 0, 125);
   ```

2. **NULL pointer:**
   ```c
   // ❌ NULL pdu
   mbc_pdu_build_read_holding_request(NULL, 1, 0, 10);
   
   // ✅ Valid pointer
   mbc_pdu_t pdu = {0};
   mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
   ```

3. **Invalid slave address:**
   ```c
   // ❌ Slave 0 (broadcast) not valid for reads
   mbc_pdu_build_read_holding_request(&pdu, 0, 0, 10);
   
   // ✅ Valid slave 1-247
   mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
   ```

---

### MBC_STATUS_CRC_ERROR (RTU)

**Problem:**
```c
mbc_status_t status = mbc_rtu_decode(buffer, len, &slave, &pdu);
// Returns MBC_STATUS_CRC_ERROR
```

**Causes & Solutions:**

1. **Serial port misconfiguration:**
   ```c
   // Check baud rate matches device
   cfsetospeed(&tty, B9600);  // Must match device
   
   // Check parity
   tty.c_cflag &= ~PARENB;  // No parity (8N1)
   
   // Check stop bits
   tty.c_cflag &= ~CSTOPB;  // 1 stop bit
   ```

2. **Noise on line:**
   ```c
   // Add retries
   for (int retry = 0; retry < 3; retry++) {
       if (mbc_rtu_decode(buffer, len, &slave, &pdu) == MBC_STATUS_OK) {
           break;
       }
       usleep(100000);  // Wait 100ms
   }
   ```

3. **Insufficient inter-frame delay:**
   ```c
   // Must wait 3.5 character times
   uint32_t delay_us = (11 * 1000000 * 3.5) / baudrate;
   usleep(delay_us);
   ```

---

### MBC_STATUS_TIMEOUT

**Problem:**
No response received from device.

**Causes & Solutions:**

1. **Device not responding:**
   ```bash
   # Test with diagnostic tool
   mbpoll -a 1 -r 0 -c 10 -t 3 /dev/ttyUSB0
   ```

2. **Wrong slave address:**
   ```c
   // Verify device address
   mbc_rtu_encode(1, &pdu, ...);  // Must match device address
   ```

3. **Increase timeout:**
   ```c
   struct timeval tv = {.tv_sec = 5};  // 5 seconds
   setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
   ```

4. **Network issues (TCP):**
   ```bash
   # Test connectivity
   ping <device_ip>
   telnet <device_ip> 502
   ```

---

## Communication Issues

### TCP Connection Refused

**Problem:**
```c
if (connect(sock, ...) < 0) {
    perror("connect");  // Connection refused
}
```

**Solutions:**

1. **Check server is running:**
   ```bash
   netstat -an | grep 502
   # Should show LISTEN on port 502
   ```

2. **Check firewall:**
   ```bash
   # Linux
   sudo ufw allow 502/tcp
   
   # Windows
   netsh advfirewall firewall add rule name="Modbus" dir=in action=allow protocol=TCP localport=502
   ```

3. **Verify IP address:**
   ```c
   // Try localhost first
   inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
   ```

4. **Check port privileges:**
   ```bash
   # Ports < 1024 require root
   sudo ./my_modbus_server
   
   # Or use port >= 1024
   mbc_tcp_connect(&tcp, "127.0.0.1", 5502);
   ```

---

### Serial Port Permission Denied

**Problem:**
```c
int fd = open("/dev/ttyUSB0", O_RDWR);
// Returns -1, errno = EACCES
```

**Solutions:**

1. **Add user to dialout group (Linux):**
   ```bash
   sudo usermod -a -G dialout $USER
   # Logout and login again
   ```

2. **Change permissions (temporary):**
   ```bash
   sudo chmod 666 /dev/ttyUSB0
   ```

3. **Create udev rule (permanent):**
   ```bash
   sudo nano /etc/udev/rules.d/50-myusb.rules
   # Add: SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", MODE="0666"
   sudo udevadm control --reload-rules
   ```

---

### RTU No Response

**Problem:**
Send RTU frame, no response received.

**Solutions:**

1. **Check wiring:**
   ```
   RS-485:
   A (TX+) ←→ A (TX+)
   B (TX-) ←→ B (TX-)
   GND     ←→ GND
   ```

2. **Enable RS-485 driver:**
   ```c
   // Set GPIO for RS-485 DE/RE
   void rs485_tx_enable(bool enable) {
       gpio_set_level(GPIO_RS485_DE, enable ? 1 : 0);
   }
   
   rs485_tx_enable(true);   // TX mode
   write(fd, buffer, len);
   tcdrain(fd);             // Wait for TX complete
   rs485_tx_enable(false);  // RX mode
   ```

3. **Check termination resistors:**
   - 120Ω at both ends of bus

4. **Verify slave address:**
   ```bash
   # Scan for devices
   for i in {1..247}; do
       mbpoll -a $i -r 0 -c 1 -t 3 /dev/ttyUSB0 2>/dev/null && echo "Found slave $i"
   done
   ```

---

### Partial or Corrupted Data

**Problem:**
Receive incomplete frames or garbage data.

**Solutions:**

1. **Flush buffers:**
   ```c
   tcflush(fd, TCIOFLUSH);  // Clear TX and RX buffers
   ```

2. **Read complete frames:**
   ```c
   uint8_t buffer[256];
   size_t total = 0;
   
   while (total < expected_length) {
       ssize_t n = read(fd, buffer + total, expected_length - total);
       if (n <= 0) break;
       total += n;
   }
   ```

3. **Use select() for timeout:**
   ```c
   fd_set readfds;
   FD_ZERO(&readfds);
   FD_SET(fd, &readfds);
   
   struct timeval tv = {.tv_sec = 1};
   if (select(fd + 1, &readfds, NULL, NULL, &tv) > 0) {
       read(fd, buffer, sizeof(buffer));
   }
   ```

---

## Performance Problems

### Slow RTU Communication

**Problem:**
RTU transactions take too long.

**Solutions:**

1. **Increase baud rate:**
   ```c
   // ❌ Slow: 9600 baud (~10 trans/sec)
   cfsetospeed(&tty, B9600);
   
   // ✅ Faster: 115200 baud (~100 trans/sec)
   cfsetospeed(&tty, B115200);
   ```

2. **Reduce inter-frame delay:**
   ```c
   // Use minimum delay for high baud rates
   if (baudrate > 19200) {
       usleep(1750);  // 1.75ms minimum
   }
   ```

3. **Batch requests:**
   ```c
   // ❌ Slow: Read registers individually
   for (int i = 0; i < 10; i++) {
       mbc_pdu_build_read_holding_request(&pdu, 1, i, 1);
       // send, receive...
   }
   
   // ✅ Fast: Read all at once
   mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
   ```

---

### High CPU Usage

**Problem:**
ModbusCore consuming excessive CPU.

**Solutions:**

1. **Don't poll in tight loop:**
   ```c
   // ❌ Bad: 100% CPU
   while (1) {
       mbc_runtime_poll(&runtime);
   }
   
   // ✅ Good: Use select/poll/sleep
   while (1) {
       mbc_runtime_poll(&runtime);
       usleep(10000);  // 10ms sleep
   }
   ```

2. **Use non-blocking I/O with select:**
   ```c
   fd_set readfds;
   FD_ZERO(&readfds);
   FD_SET(sock, &readfds);
   
   struct timeval tv = {.tv_sec = 0, .tv_usec = 100000};  // 100ms
   if (select(sock + 1, &readfds, NULL, NULL, &tv) > 0) {
       // Data available
       recv(sock, buffer, sizeof(buffer), 0);
   }
   ```

---

## Platform-Specific

### Windows Winsock Errors

**Problem:**
```c
send() returns SOCKET_ERROR
WSAGetLastError() = 10057 (WSAENOTCONN)
```

**Solutions:**

1. **Initialize Winsock:**
   ```c
   WSADATA wsaData;
   if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
       fprintf(stderr, "WSAStartup failed\n");
       return 1;
   }
   ```

2. **Clean up on exit:**
   ```c
   closesocket(sock);
   WSACleanup();
   ```

3. **Check error codes:**
   ```c
   if (send(sock, buffer, len, 0) == SOCKET_ERROR) {
       int err = WSAGetLastError();
       fprintf(stderr, "send() failed: %d\n", err);
   }
   ```

---

### macOS Serial Port Not Found

**Problem:**
```bash
$ ls /dev/ttyUSB*
ls: /dev/ttyUSB*: No such file or directory
```

**Solutions:**

1. **macOS uses different naming:**
   ```bash
   ls /dev/tty.*
   # Look for /dev/tty.usbserial-*
   ```

2. **Install driver:**
   ```bash
   # For FTDI chips
   brew install libftdi
   
   # For CH340/CH341 chips
   # Download from manufacturer
   ```

3. **Use correct device:**
   ```c
   // ❌ Linux naming
   mbc_rtu_open(&rtu, "/dev/ttyUSB0", 9600);
   
   // ✅ macOS naming
   mbc_rtu_open(&rtu, "/dev/tty.usbserial-A12345", 9600);
   ```

---

### FreeRTOS Stack Overflow

**Problem:**
```
***ERROR*** A stack overflow in task Modbus has been detected.
```

**Solutions:**

1. **Increase stack size:**
   ```c
   xTaskCreate(modbus_task, "Modbus", 4096, NULL, 5, NULL);
   //                                   ^^^^ Increase this
   ```

2. **Monitor stack usage:**
   ```c
   UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
   printf("Stack remaining: %u bytes\n", uxHighWaterMark * sizeof(StackType_t));
   ```

3. **Reduce local variables:**
   ```c
   // ❌ Large stack allocation
   uint8_t big_buffer[4096];
   
   // ✅ Use static or global
   static uint8_t big_buffer[4096];
   ```

---

## Debug Tools

### Enable Diagnostics

```c
#include <modbuscore/diagnostics.h>

mbc_diag_config_t cfg = {
    .enable_frame_tracing = true,
    .enable_error_logging = true,
    .trace_callback = my_trace_handler
};
mbc_diagnostics_init(&cfg);

void my_trace_handler(bool is_tx, const uint8_t *data, size_t len) {
    printf("[%s] ", is_tx ? "TX" : "RX");
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}
```

### Use mbpoll for Testing

```bash
# Read 10 holding registers
mbpoll -a 1 -r 0 -c 10 -t 3 /dev/ttyUSB0

# Write single register
mbpoll -a 1 -r 5 -t 3 /dev/ttyUSB0 1234
```

### Wireshark for TCP

```bash
# Capture Modbus TCP traffic
sudo tcpdump -i any -w modbus.pcap port 502

# Open in Wireshark
wireshark modbus.pcap
```

---

## Still Having Issues?

1. **Check FAQ:** [Frequently Asked Questions](faq.md)
2. **Search Issues:** [GitHub Issues](https://github.com/lgili/modbuscore/issues)
3. **Ask Question:** [GitHub Discussions](https://github.com/lgili/modbuscore/discussions)
4. **File Bug:** [New Issue](https://github.com/lgili/modbuscore/issues/new)

When reporting issues, include:
- ModbusCore version
- Platform (OS, architecture)
- Compiler version
- Minimal reproducible example
- Error messages
- Diagnostic output

---

**Prev**: [FAQ ←](faq.md) | **Next**: [Glossary →](glossary.md)
