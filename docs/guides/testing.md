---
layout: default
title: Testing Guide
parent: Guides
nav_order: 1
---

# Testing Guide
{: .no_toc }

How to test ModbusCore v1.0 and your own applications
{: .fs-6 .fw-300 }

## Table of Contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Running Built-in Tests

ModbusCore includes comprehensive unit and integration tests.

### Prerequisites

```bash
# Build ModbusCore with tests
cd modbuscore
mkdir build && cd build
cmake -DBUILD_TESTING=ON ..
make
```

### Run All Tests

```bash
# Run all tests with CTest
cd build
ctest --output-on-failure

# Or with verbose output
ctest -V
```

**Expected Output:**
```
Test project /path/to/modbuscore/build
      Start  1: runtime_smoke
 1/12 Test  #1: runtime_smoke ........................   Passed    0.01 sec
      Start  2: transport_iface_smoke
 2/12 Test  #2: transport_iface_smoke ................   Passed    0.00 sec
      Start  3: mock_transport_smoke
 3/12 Test  #3: mock_transport_smoke .................   Passed    0.00 sec
...
100% tests passed, 0 tests failed out of 12
```

### Run Specific Tests

```bash
# Run only PDU tests
ctest -R pdu

# Run only engine tests
ctest -R engine

# Run integration test
ctest -R example_tcp_client
```

---

## Test Categories

### Unit Tests

Test individual components in isolation.

**Available Tests:**
- `runtime_smoke` – Runtime dependency injection
- `transport_iface_smoke` – Transport interface wrappers
- `mock_transport_smoke` – Mock transport functionality
- `pdu_smoke` – PDU encoding/decoding
- `protocol_engine_smoke` – Engine FSM logic
- `engine_resilience_smoke` – Engine error handling

**Run:**
```bash
./build/tests/runtime_smoke
./build/tests/pdu_smoke
./build/tests/protocol_engine_smoke
```

---

### Integration Tests

Test complete end-to-end scenarios with real TCP connections.

**Test:** `example_tcp_client_fc03`

**Setup:**
```bash
# Terminal 1: Start test server
cd tests
python3 simple_tcp_server.py
# Listening on 127.0.0.1:5502

# Terminal 2: Run client
cd build/tests
./example_tcp_client_fc03
```

**Expected Output:**
```
=== ModbusCore v1.0 - TCP Client Example (FC03) ===

Step 1: Creating TCP transport...
✓ Connected to 127.0.0.1:5502

Step 2: Building runtime with DI...
✓ Runtime initialized

Step 3: Initializing protocol engine (client mode)...
✓ Engine ready

Step 4: Building FC03 request...
✓ MBAP frame encoded (13 bytes)
✓ Request submitted

Step 5: Polling for response...
✓ Response received after 3 iterations!

Step 6: Parsing response...
  Registers read:
    [0] = 0x0000 (0)
    [1] = 0x0001 (1)
    ...

=== SUCCESS ===
```

---

## Writing Your Own Tests

### Test Structure

```c
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/transport/mock.h>
#include <modbuscore/runtime/builder.h>
#include <assert.h>
#include <stdio.h>

int main(void)
{
    printf("=== My Custom Test ===\n");

    // Test code here
    // Use assert() for verification

    printf("✓ All tests passed\n");
    return 0;
}
```

### Example: Test FC06 Write Single

```c
#include <modbuscore/protocol/pdu.h>
#include <assert.h>
#include <string.h>

void test_fc06_build(void)
{
    mbc_pdu_t pdu;

    // Build FC06 request
    mbc_status_t status = mbc_pdu_build_write_single_register(&pdu, 1, 100, 1234);

    // Verify success
    assert(mbc_status_is_ok(status));

    // Verify function code
    assert(pdu.function == 0x06);

    // Verify unit ID
    assert(pdu.unit_id == 1);

    // Verify payload (address 100 = 0x0064, value 1234 = 0x04D2)
    assert(pdu.payload_length == 4);
    assert(pdu.payload[0] == 0x00);  // Address high
    assert(pdu.payload[1] == 0x64);  // Address low
    assert(pdu.payload[2] == 0x04);  // Value high
    assert(pdu.payload[3] == 0xD2);  // Value low

    printf("✓ FC06 build test passed\n");
}

void test_fc06_parse(void)
{
    // Simulate FC06 response
    mbc_pdu_t response = {
        .unit_id = 1,
        .function = 0x06,
        .payload = {0x00, 0x64, 0x04, 0xD2},  // Addr 100, Value 1234
        .payload_length = 4
    };

    uint16_t address, value;
    mbc_status_t status = mbc_pdu_parse_write_single_response(&response, &address, &value);

    assert(mbc_status_is_ok(status));
    assert(address == 100);
    assert(value == 1234);

    printf("✓ FC06 parse test passed\n");
}

int main(void)
{
    printf("=== FC06 Tests ===\n");
    test_fc06_build();
    test_fc06_parse();
    printf("=== All Tests Passed ===\n");
    return 0;
}
```

### Add to CMake

```cmake
# In tests/CMakeLists.txt
add_executable(my_fc06_test my_fc06_test.c)
target_link_libraries(my_fc06_test PRIVATE modbuscore)
add_test(NAME my_fc06_test COMMAND my_fc06_test)
```

---

## Formatting & Static Analysis

Consistência de estilo ajuda a manter o repositório saudável:

```bash
# Ajuste formatação antes de commitar
clang-format -i src/transport/posix_tcp.c include/modbuscore/transport/posix_tcp.h

# Rodar clang-tidy (adicione include paths conforme necessário)
clang-tidy src/protocol/engine.c -- -Iinclude -std=c17
```

Ambos utilizam `.clang-format` e `.clang-tidy` fornecidos no repositório. Em CI
executamos as mesmas configurações; rodá-las localmente evita divergências.

---

## Testing with Mock Transport

Mock transport allows testing without real network connections.

### Example: Test Engine with Mock

```c
#include <modbuscore/transport/mock.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/runtime/builder.h>

void test_engine_with_mock(void)
{
    // Create mock transport
    mbc_transport_iface_t transport;
    mbc_mock_ctx_t *mock_ctx;
    mbc_mock_create(&transport, &mock_ctx);

    // Build runtime
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);

    // Initialize engine
    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,
        .response_timeout_ms = 1000,
    };
    mbc_engine_init(&engine, &config);

    // Prepare mock response
    uint8_t mock_response[] = {
        0x00, 0x01,         // Transaction ID
        0x00, 0x00,         // Protocol ID
        0x00, 0x05,         // Length
        0x01,               // Unit ID
        0x03,               // FC03
        0x02,               // Byte count
        0x00, 0x0A          // Register value = 10
    };

    mbc_mock_queue_response(mock_ctx, mock_response, sizeof(mock_response));

    // Build and send request
    mbc_pdu_t request;
    mbc_pdu_build_read_holding_request(&request, 1, 0, 1);

    // Encode MBAP...
    // Submit request...

    // Poll for response
    for (int i = 0; i < 10; ++i) {
        mbc_engine_step(&engine, 100);

        mbc_pdu_t response;
        if (mbc_engine_take_pdu(&engine, &response)) {
            // Parse response
            const uint8_t *data;
            size_t count;
            mbc_pdu_parse_read_holding_response(&response, &data, &count);

            uint16_t value = (data[0] << 8) | data[1];
            assert(value == 10);

            printf("✓ Mock test passed: received value = %u\n", value);
            break;
        }
    }

    // Cleanup
    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_mock_destroy(mock_ctx);
}
```

---

## Testing Against Real Servers

### Option 1: Python pymodbus Server

```bash
# Install pymodbus
pip install pymodbus

# Run server
pymodbus.server --host 0.0.0.0 --port 502
```

### Option 2: Docker Modbus Simulator

```bash
# Run containerized Modbus server
docker run -d -p 502:502 oitc/modbus-server

# Test connection
./build/tests/example_tcp_client_fc03
```

### Option 3: Hardware PLC

Configure client to connect to your PLC:

```c
mbc_posix_tcp_config_t tcp_config = {
    .host = "192.168.1.100",  // Your PLC IP
    .port = 502,
    .connect_timeout_ms = 5000,
    .recv_timeout_ms = 2000,
};
```

---

## Continuous Integration

### GitHub Actions Example

```yaml
# .github/workflows/ci.yml
name: CI

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v2

    - name: Install dependencies
      run: sudo apt-get install -y cmake build-essential

    - name: Build
      run: |
        mkdir build
        cd build
        cmake -DBUILD_TESTING=ON ..
        make

    - name: Run tests
      run: |
        cd build
        ctest --output-on-failure

    - name: Run integration test
      run: |
        cd tests
        python3 simple_tcp_server.py &
        SERVER_PID=$!
        sleep 2
        ../build/tests/example_tcp_client_fc03
        kill $SERVER_PID
```

---

## Test Coverage

### Measure Code Coverage (GCC/Clang)

```bash
# Build with coverage flags
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="--coverage" \
  -DBUILD_TESTING=ON

cmake --build build

# Run tests
cd build
ctest

# Generate coverage report
gcov ../src/**/*.c
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html

# View report
open coverage_html/index.html
```

---

## Performance Testing

### Measure Request/Response Latency

```c
#include <time.h>

uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
}

void benchmark_fc03(void)
{
    uint64_t start = get_time_ms();

    // Send FC03 request
    // ... (send and receive)

    uint64_t end = get_time_ms();
    printf("FC03 latency: %llu ms\n", end - start);
}
```

### Throughput Test

```c
void benchmark_throughput(void)
{
    const int num_requests = 100;
    uint64_t start = get_time_ms();

    for (int i = 0; i < num_requests; ++i) {
        // Send FC03 request
        // Wait for response
    }

    uint64_t end = get_time_ms();
    uint64_t duration_ms = end - start;
    double requests_per_sec = (num_requests * 1000.0) / duration_ms;

    printf("Throughput: %.2f requests/sec\n", requests_per_sec);
}
```

---

## Best Practices

### ✅ DO

- **Test edge cases** (0 quantity, max quantity, invalid addresses)
- **Test error conditions** (timeouts, exceptions, invalid data)
- **Use mock transport** for fast, deterministic tests
- **Verify cleanup** (no memory leaks, proper resource release)
- **Add integration tests** for new features

### ❌ DON'T

- **Rely only on integration tests** – They're slow and fragile
- **Ignore test failures** – Fix them immediately
- **Skip cleanup code in tests** – May leak resources
- **Hardcode IP addresses** – Use environment variables or configs

---

## Debugging Failed Tests

### Enable Verbose Output

```bash
# Run single test with debug output
./build/tests/protocol_engine_smoke

# Run with CTest verbose mode
ctest -V -R protocol_engine
```

### Use GDB

```bash
# Debug failing test
gdb ./build/tests/protocol_engine_smoke

(gdb) run
# Wait for crash...
(gdb) backtrace
(gdb) print variable_name
```

### Add Debug Prints

```c
void test_something(void)
{
    printf("DEBUG: Before engine_step\n");
    mbc_status_t status = mbc_engine_step(&engine, 10);
    printf("DEBUG: After engine_step, status=%d\n", status);

    assert(mbc_status_is_ok(status));
}
```

---

## Related Documentation

- **[API Reference](../api/)** – Function documentation
- **[Troubleshooting](troubleshooting.md)** – Common issues
- **[Quick Start](../quick-start.md)** – Getting started
