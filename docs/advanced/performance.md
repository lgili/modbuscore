# Performance Tuning Guide

Comprehensive guide to optimizing ModbusCore for maximum performance.

## Table of Contents

- [Overview](#overview)
- [Profiling](#profiling)
- [Memory Optimization](#memory-optimization)
- [CPU Optimization](#cpu-optimization)
- [Network Tuning](#network-tuning)
- [Zero-Copy Techniques](#zero-copy-techniques)
- [Benchmarking](#benchmarking)
- [Platform-Specific Tips](#platform-specific-tips)

## Overview

ModbusCore is designed for **efficiency** with:

- Zero heap allocations by default
- Minimal memory footprint
- Fast CRC calculation
- Efficient encoding/decoding
- Non-blocking I/O

**Performance Targets:**

| Metric | Desktop (x86) | ARM Cortex-M4 |
|--------|---------------|---------------|
| PDU encoding | ~200 ns | ~2 µs |
| MBAP encoding | ~300 ns | ~4 µs |
| RTU CRC | ~500 ns | ~6 µs |
| Full transaction | ~500 µs | ~5 ms |

---

## Profiling

### Tools

| Platform | Profiler | Usage |
|----------|----------|-------|
| **Linux** | `perf`, `valgrind` | CPU profiling, cache analysis |
| **macOS** | Instruments | Time profiler, allocations |
| **Windows** | Visual Studio Profiler | CPU sampling, memory |
| **Embedded** | Segger SystemView | Real-time tracing |

### Using perf (Linux)

```bash
# Compile with debug symbols
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build

# Profile your application
perf record -g ./your_modbus_app

# View results
perf report
```

### Using valgrind (Linux)

```bash
# CPU profiling with callgrind
valgrind --tool=callgrind ./your_modbus_app

# Visualize with kcachegrind
kcachegrind callgrind.out.*
```

### Using Instruments (macOS)

```bash
# Launch with Time Profiler
instruments -t "Time Profiler" ./your_modbus_app
```

---

## Memory Optimization

### Stack Usage

ModbusCore uses **stack allocation** for buffers:

```c
// Typical stack usage per function
mbc_pdu_t pdu;                    // ~260 bytes
uint8_t buffer[MBC_PDU_MAX_SIZE]; // 260 bytes
mbc_mbap_header_t hdr;            // ~16 bytes

// Total: ~540 bytes per transaction
```

**Optimization:**

```c
// ✅ Reuse buffers across transactions
static uint8_t tx_buffer[260];
static uint8_t rx_buffer[260];

// Avoid repeated stack allocation
for (int i = 0; i < 1000; i++) {
    mbc_pdu_build_read_holding_request(&pdu, 1, i, 10);
    mbc_mbap_encode(&hdr, &pdu, tx_buffer, sizeof(tx_buffer), &length);
    // Send...
}
```

### Heap Usage

ModbusCore has **zero heap allocations** by default:

```c
// No malloc/free in these operations
mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
mbc_mbap_encode(&hdr, &pdu, buffer, sizeof(buffer), &length);
mbc_rtu_calculate_crc(buffer, length);
```

**Custom allocator (if needed):**

```c
// Use memory pool for deterministic allocation
void *pool_alloc(void *ctx, size_t size) {
    return memory_pool_allocate((pool_t*)ctx, size);
}

void pool_free(void *ctx, void *ptr) {
    memory_pool_free((pool_t*)ctx, ptr);
}

mbc_allocator_iface_t allocator = {
    .ctx = &my_pool,
    .alloc = pool_alloc,
    .free = pool_free
};

mbc_runtime_builder_with_allocator(&builder, &allocator);
```

### Memory Footprint

| Component | Flash (code) | RAM (data) |
|-----------|--------------|------------|
| **Protocol (PDU/MBAP/RTU)** | ~2 KB | ~0 bytes (stack only) |
| **Transport (TCP)** | ~3 KB | ~1 KB (context) |
| **Transport (RTU)** | ~4 KB | ~512 bytes (buffers) |
| **Runtime** | ~1 KB | ~200 bytes |
| **Diagnostics** | ~2 KB | ~500 bytes |
| **Total (typical)** | ~12 KB | ~2 KB |

---

## CPU Optimization

### Fast Path Optimization

```c
// ✅ Good: Minimal overhead
if (status == MBC_STATUS_OK) {
    // Fast path - no branching
    process_response(&pdu);
}

// ❌ Avoid: Excessive error checking in hot loop
if (status != MBC_STATUS_OK) {
    log_error(status);
    if (status == MBC_STATUS_TIMEOUT) {
        retry();
    } else if (status == MBC_STATUS_CRC_ERROR) {
        handle_crc_error();
    }
    // ... many branches
}
```

### CRC Optimization

ModbusCore uses **table-based CRC-16** for speed:

```c
// Fast: ~500ns on x86, ~6µs on ARM Cortex-M4
uint16_t crc = mbc_rtu_calculate_crc(buffer, length);
```

**Further optimization (inline assembly):**

```c
// Platform-specific optimized CRC
#ifdef __ARM_NEON
uint16_t fast_crc_neon(const uint8_t *data, size_t len);
#endif
```

### Batch Operations

```c
// ❌ Slow: Multiple single writes
for (int i = 0; i < 100; i++) {
    mbc_pdu_build_write_single_request(&pdu, 1, i, values[i]);
    // Send...
}

// ✅ Fast: Single multi-write
mbc_pdu_build_write_multiple_request(&pdu, 1, 0, values, 100);
// Send once
```

### Compiler Optimization

```cmake
# CMakeLists.txt
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(modbuscore PRIVATE
        -O3                    # Maximum optimization
        -march=native          # CPU-specific instructions
        -flto                  # Link-time optimization
        -ffast-math            # Fast floating-point (if used)
    )
endif()
```

---

## Network Tuning

### TCP Performance

#### Socket Buffer Sizes

```c
// Increase socket buffers for high throughput
int sock = /* ... */;

int sndbuf = 256 * 1024;  // 256 KB send buffer
int rcvbuf = 256 * 1024;  // 256 KB receive buffer

setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
```

#### TCP_NODELAY (Disable Nagle)

```c
// Reduce latency for small packets
int flag = 1;
setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
```

#### Non-Blocking I/O

```c
// Use non-blocking for better concurrency
#include <fcntl.h>

int flags = fcntl(sock, F_GETFL, 0);
fcntl(sock, F_SETFL, flags | O_NONBLOCK);
```

### RTU Performance

#### Baud Rate Selection

| Baud Rate | Char Time | Frame Time (10 bytes) |
|-----------|-----------|------------------------|
| 9600 | ~1 ms | ~10 ms |
| 19200 | ~0.5 ms | ~5 ms |
| 38400 | ~0.25 ms | ~2.5 ms |
| 115200 | ~0.087 ms | ~0.87 ms |

**Recommendation:** Use **19200+ baud** for better performance.

#### Guard Time

```c
// Auto-calculate optimal guard time
mbc_posix_rtu_config_t cfg = {
    .device_path = "/dev/ttyUSB0",
    .baud_rate = 19200,
    .guard_time_us = 0  // Auto: 3.5 char times
};

// Manual override for faster polling
cfg.guard_time_us = 1000;  // 1ms (aggressive)
```

---

## Zero-Copy Techniques

### Direct Buffer Access

```c
// ✅ Zero-copy: Encode directly into TX buffer
uint8_t tx_buffer[260];
size_t tx_length;

mbc_pdu_t pdu;
mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);

mbc_mbap_header_t hdr = { .transaction_id = 1, .unit_id = 1 };
mbc_mbap_encode(&hdr, &pdu, tx_buffer, sizeof(tx_buffer), &tx_length);

// Send directly (zero-copy)
send(sock, tx_buffer, tx_length, 0);
```

### DMA-Friendly Buffers

```c
// Aligned buffers for DMA
__attribute__((aligned(64))) static uint8_t dma_tx_buffer[260];
__attribute__((aligned(64))) static uint8_t dma_rx_buffer[260];

// Use with DMA controllers
dma_transfer(dma_tx_buffer, tx_length);
```

### Scatter-Gather I/O

```c
// Use vectored I/O for zero-copy
#include <sys/uio.h>

struct iovec iov[2];
iov[0].iov_base = mbap_header;
iov[0].iov_len = 7;
iov[1].iov_base = pdu_data;
iov[1].iov_len = pdu_length;

writev(sock, iov, 2);  // Single syscall, no copy
```

---

## Benchmarking

### Micro-Benchmarks

```c
#include <time.h>

uint64_t benchmark_pdu_encoding(int iterations) {
    struct timespec start, end;
    mbc_pdu_t pdu;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < iterations; i++) {
        mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL
                        + (end.tv_nsec - start.tv_nsec);
    
    return elapsed_ns / iterations;  // ns per operation
}

int main(void) {
    uint64_t avg_ns = benchmark_pdu_encoding(1000000);
    printf("PDU encoding: %llu ns\n", avg_ns);
    return 0;
}
```

### Throughput Testing

```c
// Measure transactions per second
uint64_t start_ms = get_time_ms();
int transactions = 0;

while (transactions < 10000) {
    // Build request
    mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    mbc_mbap_encode(&hdr, &pdu, buffer, sizeof(buffer), &length);
    
    // Send/receive
    send(sock, buffer, length, 0);
    recv(sock, buffer, sizeof(buffer), 0);
    
    transactions++;
}

uint64_t elapsed_ms = get_time_ms() - start_ms;
double tps = (double)transactions / (elapsed_ms / 1000.0);

printf("Throughput: %.2f transactions/sec\n", tps);
```

### Latency Measurement

```c
// Measure round-trip latency
struct timespec start, end;

clock_gettime(CLOCK_MONOTONIC, &start);

// Send request
send(sock, tx_buffer, tx_length, 0);

// Receive response
recv(sock, rx_buffer, sizeof(rx_buffer), 0);

clock_gettime(CLOCK_MONOTONIC, &end);

uint64_t latency_us = ((end.tv_sec - start.tv_sec) * 1000000ULL
                     + (end.tv_nsec - start.tv_nsec) / 1000);

printf("Round-trip latency: %llu µs\n", latency_us);
```

---

## Platform-Specific Tips

### Linux

```c
// Use epoll for high-performance I/O multiplexing
int epoll_fd = epoll_create1(0);

struct epoll_event ev;
ev.events = EPOLLIN | EPOLLOUT | EPOLLET;  // Edge-triggered
ev.data.fd = sock;
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev);

// Wait for events
struct epoll_event events[10];
int nfds = epoll_wait(epoll_fd, events, 10, timeout_ms);
```

### Windows

```c
// Use IOCP for scalable async I/O
HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

// Associate socket with IOCP
CreateIoCompletionPort((HANDLE)sock, iocp, (ULONG_PTR)sock, 0);

// Post async receive
WSAOVERLAPPED overlapped = {0};
WSABUF buf = { sizeof(buffer), (char*)buffer };
WSARecv(sock, &buf, 1, NULL, &flags, &overlapped, NULL);

// Wait for completion
DWORD bytes;
ULONG_PTR key;
LPOVERLAPPED lpOverlapped;
GetQueuedCompletionStatus(iocp, &bytes, &key, &lpOverlapped, INFINITE);
```

### Embedded (ARM Cortex-M)

```c
// Enable CPU cache (if available)
SCB_EnableICache();
SCB_EnableDCache();

// Use fast memory (SRAM) for buffers
__attribute__((section(".sram")))
static uint8_t fast_buffer[260];

// Compiler optimization hints
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#define HOT_FUNCTION __attribute__((hot))
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

HOT_FUNCTION
ALWAYS_INLINE
static void fast_encode(mbc_pdu_t *pdu) {
    if (LIKELY(pdu != NULL)) {
        // Fast path
    }
}
```

---

## Performance Checklist

### ✅ Code Optimization

- [ ] Compile with `-O3` (or equivalent)
- [ ] Enable link-time optimization (`-flto`)
- [ ] Use CPU-specific instructions (`-march=native`)
- [ ] Reuse buffers (avoid repeated allocation)
- [ ] Batch operations when possible
- [ ] Minimize branching in hot paths

### ✅ Memory Optimization

- [ ] Use stack allocation (default)
- [ ] Align DMA buffers properly
- [ ] Avoid unnecessary copying
- [ ] Use zero-copy techniques
- [ ] Monitor stack usage

### ✅ Network Optimization

- [ ] Enable `TCP_NODELAY` for low latency
- [ ] Increase socket buffer sizes for throughput
- [ ] Use non-blocking I/O
- [ ] Implement connection pooling
- [ ] Use multiplexing (epoll/IOCP)

### ✅ RTU Optimization

- [ ] Use higher baud rates (19200+)
- [ ] Tune guard time appropriately
- [ ] Implement interrupt-driven I/O
- [ ] Use DMA for serial transfers

### ✅ Profiling

- [ ] Profile with real workloads
- [ ] Identify hot spots
- [ ] Measure before and after optimization
- [ ] Test on target hardware

---

## Performance Anti-Patterns

### ❌ Excessive Logging in Hot Path

```c
// ❌ Bad: Logging in tight loop
for (int i = 0; i < 10000; i++) {
    printf("Sending request %d\n", i);  // SLOW
    send_request();
}

// ✅ Good: Minimal logging
for (int i = 0; i < 10000; i++) {
    send_request();
}
printf("Sent 10000 requests\n");
```

### ❌ Repeated String Operations

```c
// ❌ Bad: String formatting in loop
for (int i = 0; i < 1000; i++) {
    char buf[100];
    sprintf(buf, "Request %d", i);  // SLOW
}

// ✅ Good: Pre-format or avoid
for (int i = 0; i < 1000; i++) {
    // Just use integer directly
}
```

### ❌ Unnecessary Blocking

```c
// ❌ Bad: Blocking in tight loop
for (int i = 0; i < 100; i++) {
    send_request();
    sleep(1);  // WASTE
}

// ✅ Good: Use timeouts or async I/O
set_socket_timeout(sock, 1000);
for (int i = 0; i < 100; i++) {
    send_request_nonblocking();
}
```

---

## Benchmarking Results

### Typical Performance (Intel i7, Ubuntu 22.04)

| Operation | Time | Ops/sec |
|-----------|------|---------|
| PDU build | 180 ns | 5.5 M |
| MBAP encode | 250 ns | 4.0 M |
| RTU CRC | 450 ns | 2.2 M |
| TCP send/recv (local) | 120 µs | 8,300 |
| RTU @ 19200 baud | 5 ms | 200 |

### Embedded (ARM Cortex-M4 @ 80MHz)

| Operation | Time | Ops/sec |
|-----------|------|---------|
| PDU build | 2 µs | 500 K |
| MBAP encode | 4 µs | 250 K |
| RTU CRC | 6 µs | 166 K |
| Transaction (TCP) | 5 ms | 200 |

---

## Next Steps

- [Security Guide](security.md) - Secure your deployments
- [Embedded Guide](embedded.md) - Optimize for embedded platforms
- [API Reference](../api/README.md) - Detailed API documentation

---

**Prev**: [Advanced Topics ←](README.md) | **Next**: [Security →](security.md)
