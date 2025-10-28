# Testing Guide

Complete guide to testing Modbus applications with ModbusCore.

## Table of Contents

- [Overview](#overview)
- [Testing Strategies](#testing-strategies)
- [Mock Transport](#mock-transport)
- [Unit Testing](#unit-testing)
- [Integration Testing](#integration-testing)
- [Test Examples](#test-examples)
- [Best Practices](#best-practices)

## Overview

ModbusCore provides comprehensive testing support through:

- **Mock transport** for simulating responses
- **Loopback transport** for testing without hardware
- **Dependency injection** for test doubles
- **Deterministic behavior** (no hidden state)

**Key Features:**
- No external dependencies for unit tests
- Scriptable mock responses
- Integration with standard test frameworks
- Hardware-in-the-loop (HIL) support

---

## Testing Strategies

### Test Pyramid

```
         ┌────────────┐
         │  End-to-End│  (Few, slow, with real devices)
         │   Tests    │
         └────────────┘
       ┌──────────────────┐
       │  Integration     │  (Some, with loopback/mock)
       │    Tests         │
       └──────────────────┘
    ┌────────────────────────┐
    │   Unit Tests           │  (Many, fast, isolated)
    │   (Mock transport)     │
    └────────────────────────┘
```

### Test Types

| Test Type | Transport | Purpose | Speed |
|-----------|-----------|---------|-------|
| **Unit** | Mock | Test protocol logic | Fast (µs) |
| **Integration** | Loopback | Test full stack | Medium (ms) |
| **System** | Real TCP/RTU | Test with devices | Slow (100ms+) |
| **HIL** | Real hardware | Validate physical layer | Slow (seconds) |

---

## Mock Transport

The mock transport allows **scripted responses** for predictable testing.

### Header

```c
#include <modbuscore/transport/mock.h>
```

### Creating Mock Transport

```c
mbc_transport_iface_t iface;
mbc_mock_transport_ctx_t *ctx = NULL;

mbc_status_t status = mbc_mock_transport_create(&iface, &ctx);
if (status != MBC_STATUS_OK) {
    // Handle error
}
```

### Adding Responses

```c
// Response 1: Read holding registers (10 values)
uint8_t response1[] = {
    0x00, 0x01,  // Transaction ID
    0x00, 0x00,  // Protocol ID
    0x00, 0x17,  // Length
    0x01,        // Unit ID
    0x03,        // Function code
    0x14,        // Byte count (20 bytes = 10 registers)
    0x00, 0x64,  // Register 0: 100
    0x00, 0xC8,  // Register 1: 200
    // ... more registers ...
};

mbc_mock_transport_add_response(ctx, response1, sizeof(response1));
```

### Using Mock in Tests

```c
void test_read_holding_registers(void) {
    // Setup
    mbc_transport_iface_t iface;
    mbc_mock_transport_ctx_t *ctx;
    mbc_mock_transport_create(&iface, &ctx);
    
    // Configure expected response
    uint8_t expected_response[] = { /* ... */ };
    mbc_mock_transport_add_response(ctx, expected_response, sizeof(expected_response));
    
    // Build runtime with mock
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);
    
    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);
    
    // Test: Send request
    mbc_pdu_t request;
    mbc_pdu_build_read_holding_request(&request, 1, 0, 10);
    
    // Encode and send
    mbc_mbap_header_t hdr = { .transaction_id = 1, .unit_id = 1 };
    uint8_t buffer[260];
    size_t length;
    mbc_mbap_encode(&hdr, &request, buffer, sizeof(buffer), &length);
    
    mbc_transport_io_t io;
    mbc_status_t status = mbc_transport_send(&iface, buffer, length, &io);
    
    // Assert
    assert(status == MBC_STATUS_OK);
    assert(io.processed == length);
    
    // Receive response
    uint8_t rx_buffer[260];
    status = mbc_transport_receive(&iface, rx_buffer, sizeof(rx_buffer), &io);
    
    assert(status == MBC_STATUS_OK);
    assert(io.processed > 0);
    
    // Cleanup
    mbc_runtime_shutdown(&runtime);
    mbc_mock_transport_destroy(ctx);
}
```

---

## Unit Testing

Unit tests verify **isolated components** without external dependencies.

### Testing Protocol Functions

```c
#include <modbuscore/protocol/pdu.h>
#include <assert.h>

void test_pdu_build_read_holding(void) {
    mbc_pdu_t pdu;
    
    // Test valid parameters
    mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    
    assert(status == MBC_STATUS_OK);
    assert(pdu.function == 0x03);
    assert(pdu.data[0] == 0x00);  // Start address high
    assert(pdu.data[1] == 0x00);  // Start address low
    assert(pdu.data[2] == 0x00);  // Quantity high
    assert(pdu.data[3] == 0x0A);  // Quantity low (10)
}

void test_pdu_build_invalid_quantity(void) {
    mbc_pdu_t pdu;
    
    // Test invalid quantity (> 125)
    mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 200);
    
    assert(status == MBC_STATUS_INVALID_PARAMETER);
}

void test_pdu_parse_response(void) {
    mbc_pdu_t response;
    response.function = 0x03;
    response.data[0] = 0x04;  // Byte count
    response.data[1] = 0x00;
    response.data[2] = 0x64;
    response.data[3] = 0x00;
    response.data[4] = 0xC8;
    response.data_length = 5;
    
    uint16_t values[2];
    size_t count;
    
    mbc_status_t status = mbc_pdu_parse_read_response(&response, values, 2, &count);
    
    assert(status == MBC_STATUS_OK);
    assert(count == 2);
    assert(values[0] == 100);
    assert(values[1] == 200);
}

int main(void) {
    test_pdu_build_read_holding();
    test_pdu_build_invalid_quantity();
    test_pdu_parse_response();
    
    printf("All tests passed!\n");
    return 0;
}
```

### Testing MBAP Encoding

```c
#include <modbuscore/protocol/mbap.h>
#include <assert.h>
#include <string.h>

void test_mbap_encode(void) {
    mbc_pdu_t pdu;
    mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    
    mbc_mbap_header_t hdr = {
        .transaction_id = 0x1234,
        .protocol_id = 0,
        .unit_id = 1
    };
    
    uint8_t buffer[260];
    size_t length;
    
    mbc_status_t status = mbc_mbap_encode(&hdr, &pdu, buffer, sizeof(buffer), &length);
    
    assert(status == MBC_STATUS_OK);
    assert(length == 12);  // 7 (MBAP) + 5 (PDU)
    assert(buffer[0] == 0x12);  // Transaction ID high
    assert(buffer[1] == 0x34);  // Transaction ID low
    assert(buffer[6] == 0x01);  // Unit ID
    assert(buffer[7] == 0x03);  // Function code
}

void test_mbap_decode(void) {
    uint8_t frame[] = {
        0x00, 0x01,  // Transaction ID
        0x00, 0x00,  // Protocol ID
        0x00, 0x06,  // Length
        0x01,        // Unit ID
        0x03,        // Function code
        0x00, 0x00,  // Start address
        0x00, 0x0A   // Quantity
    };
    
    mbc_mbap_header_t hdr;
    mbc_pdu_t pdu;
    
    mbc_status_t status = mbc_mbap_decode(frame, sizeof(frame), &hdr, &pdu);
    
    assert(status == MBC_STATUS_OK);
    assert(hdr.transaction_id == 1);
    assert(hdr.unit_id == 1);
    assert(pdu.function == 0x03);
}
```

### Testing CRC

```c
#include <modbuscore/protocol/crc.h>
#include <assert.h>

void test_crc_calculation(void) {
    uint8_t data[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x0A };
    
    uint16_t crc = mbc_rtu_calculate_crc(data, sizeof(data));
    
    // Expected CRC for this frame
    assert(crc == 0xC5CD);
}

void test_crc_verification(void) {
    uint8_t frame[] = {
        0x01, 0x03, 0x00, 0x00, 0x00, 0x0A,
        0xCD, 0xC5  // CRC (little-endian)
    };
    
    bool valid = mbc_rtu_verify_crc(frame, sizeof(frame));
    
    assert(valid == true);
}
```

---

## Integration Testing

Integration tests verify **multiple components** working together.

### Full Transaction Test

```c
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/mock.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>

void test_full_transaction(void) {
    // Setup mock transport
    mbc_transport_iface_t iface;
    mbc_mock_transport_ctx_t *mock_ctx;
    mbc_mock_transport_create(&iface, &mock_ctx);
    
    // Configure expected response
    uint8_t response[] = {
        0x00, 0x01,  // Transaction ID
        0x00, 0x00,  // Protocol ID
        0x00, 0x07,  // Length
        0x01,        // Unit ID
        0x03,        // Function code
        0x04,        // Byte count
        0x00, 0x64,  // Value 1: 100
        0x00, 0xC8   // Value 2: 200
    };
    mbc_mock_transport_add_response(mock_ctx, response, sizeof(response));
    
    // Build runtime
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);
    
    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);
    
    // Build request
    mbc_pdu_t request;
    mbc_pdu_build_read_holding_request(&request, 1, 0, 2);
    
    mbc_mbap_header_t req_hdr = {
        .transaction_id = 1,
        .unit_id = 1
    };
    
    uint8_t tx_buffer[260];
    size_t tx_length;
    mbc_mbap_encode(&req_hdr, &request, tx_buffer, sizeof(tx_buffer), &tx_length);
    
    // Send request
    mbc_transport_io_t io;
    mbc_status_t status = mbc_transport_send(&iface, tx_buffer, tx_length, &io);
    assert(status == MBC_STATUS_OK);
    
    // Receive response
    uint8_t rx_buffer[260];
    status = mbc_transport_receive(&iface, rx_buffer, sizeof(rx_buffer), &io);
    assert(status == MBC_STATUS_OK);
    
    // Decode response
    mbc_mbap_header_t resp_hdr;
    mbc_pdu_t resp_pdu;
    status = mbc_mbap_decode(rx_buffer, io.processed, &resp_hdr, &resp_pdu);
    assert(status == MBC_STATUS_OK);
    
    // Parse values
    uint16_t values[2];
    size_t count;
    status = mbc_pdu_parse_read_response(&resp_pdu, values, 2, &count);
    assert(status == MBC_STATUS_OK);
    assert(count == 2);
    assert(values[0] == 100);
    assert(values[1] == 200);
    
    printf("✓ Full transaction test passed\n");
    
    // Cleanup
    mbc_runtime_shutdown(&runtime);
    mbc_mock_transport_destroy(mock_ctx);
}
```

### Testing Error Handling

```c
void test_exception_response(void) {
    // Setup
    mbc_transport_iface_t iface;
    mbc_mock_transport_ctx_t *mock_ctx;
    mbc_mock_transport_create(&iface, &mock_ctx);
    
    // Configure exception response (illegal function)
    uint8_t exception[] = {
        0x00, 0x01,  // Transaction ID
        0x00, 0x00,  // Protocol ID
        0x00, 0x03,  // Length
        0x01,        // Unit ID
        0x83,        // Exception: 0x03 | 0x80
        0x01         // Exception code: Illegal function
    };
    mbc_mock_transport_add_response(mock_ctx, exception, sizeof(exception));
    
    // Send request...
    // Receive response...
    
    // Decode and check for exception
    mbc_mbap_header_t hdr;
    mbc_pdu_t pdu;
    mbc_mbap_decode(rx_buffer, length, &hdr, &pdu);
    
    assert(pdu.function == 0x83);  // Exception flag set
    assert(pdu.data[0] == 0x01);   // Illegal function code
    
    printf("✓ Exception handling test passed\n");
    
    // Cleanup...
}
```

---

## Test Examples

### Using Google Test (C++)

```cpp
#include <gtest/gtest.h>
#include <modbuscore/protocol/pdu.h>

TEST(PDUTest, BuildReadHoldingRequest) {
    mbc_pdu_t pdu;
    
    mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    
    EXPECT_EQ(status, MBC_STATUS_OK);
    EXPECT_EQ(pdu.function, 0x03);
    EXPECT_EQ(pdu.data_length, 4);
}

TEST(PDUTest, InvalidQuantity) {
    mbc_pdu_t pdu;
    
    mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 200);
    
    EXPECT_EQ(status, MBC_STATUS_INVALID_PARAMETER);
}

TEST(MBAPTest, EncodeAndDecode) {
    mbc_pdu_t pdu;
    mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    
    mbc_mbap_header_t hdr = { .transaction_id = 1, .unit_id = 1 };
    uint8_t buffer[260];
    size_t length;
    
    mbc_mbap_encode(&hdr, &pdu, buffer, sizeof(buffer), &length);
    
    mbc_mbap_header_t decoded_hdr;
    mbc_pdu_t decoded_pdu;
    
    mbc_mbap_decode(buffer, length, &decoded_hdr, &decoded_pdu);
    
    EXPECT_EQ(decoded_hdr.transaction_id, 1);
    EXPECT_EQ(decoded_hdr.unit_id, 1);
    EXPECT_EQ(decoded_pdu.function, 0x03);
}
```

### Using Unity (C)

```c
#include "unity.h"
#include <modbuscore/protocol/pdu.h>

void setUp(void) {
    // Setup before each test
}

void tearDown(void) {
    // Cleanup after each test
}

void test_pdu_build_read_holding(void) {
    mbc_pdu_t pdu;
    
    mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    
    TEST_ASSERT_EQUAL(MBC_STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT8(0x03, pdu.function);
}

void test_pdu_invalid_quantity(void) {
    mbc_pdu_t pdu;
    
    mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 200);
    
    TEST_ASSERT_EQUAL(MBC_STATUS_INVALID_PARAMETER, status);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_pdu_build_read_holding);
    RUN_TEST(test_pdu_invalid_quantity);
    
    return UNITY_END();
}
```

---

## Best Practices

### 1. Use Mock Transport for Unit Tests

```c
// ✅ Good: Fast, isolated tests
mbc_mock_transport_create(&iface, &ctx);
mbc_mock_transport_add_response(ctx, expected, len);

// ❌ Avoid: Real network in unit tests
mbc_posix_tcp_create(&cfg, &iface, &ctx);
```

### 2. Test Error Paths

```c
// Test success case
TEST(test_success) { ... }

// Test error cases
TEST(test_invalid_parameter) { ... }
TEST(test_buffer_too_small) { ... }
TEST(test_crc_error) { ... }
TEST(test_timeout) { ... }
```

### 3. Verify All Fields

```c
// ✅ Good: Check all important fields
assert(pdu.function == 0x03);
assert(pdu.data_length == 4);
assert(pdu.data[0] == 0x00);
assert(pdu.data[1] == 0x00);

// ❌ Incomplete: Only checking status
assert(status == MBC_STATUS_OK);
```

### 4. Clean Up Resources

```c
void test_something(void) {
    mbc_transport_iface_t iface;
    mbc_mock_transport_ctx_t *ctx;
    
    mbc_mock_transport_create(&iface, &ctx);
    
    // ... test code ...
    
    // Always cleanup
    mbc_mock_transport_destroy(ctx);
}
```

### 5. Use Fixtures for Common Setup

```c
typedef struct {
    mbc_runtime_t runtime;
    mbc_transport_iface_t iface;
    mbc_mock_transport_ctx_t *mock_ctx;
} test_fixture_t;

test_fixture_t setup_fixture(void) {
    test_fixture_t f;
    
    mbc_mock_transport_create(&f.iface, &f.mock_ctx);
    
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &f.iface);
    mbc_runtime_builder_build(&builder, &f.runtime);
    
    return f;
}

void teardown_fixture(test_fixture_t *f) {
    mbc_runtime_shutdown(&f->runtime);
    mbc_mock_transport_destroy(f->mock_ctx);
}

// Usage
void test_with_fixture(void) {
    test_fixture_t f = setup_fixture();
    
    // Run test...
    
    teardown_fixture(&f);
}
```

---

## Running Tests

### CMake + CTest

```bash
# Configure with tests enabled
cmake -S . -B build -DBUILD_TESTING=ON

# Build tests
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test
ctest --test-dir build -R test_pdu
```

### Manual Compilation

```bash
# Compile single test
gcc -o test_pdu test_pdu.c -I../include -L../lib -lmodbuscore

# Run test
./test_pdu
```

---

## Continuous Integration

### GitHub Actions Example

```yaml
name: Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
      - uses: actions/checkout@v3
      
      - name: Configure
        run: cmake -S . -B build -DBUILD_TESTING=ON
      
      - name: Build
        run: cmake --build build
      
      - name: Test
        run: ctest --test-dir build --output-on-failure
```

---

## Next Steps

- [Runtime Guide](runtime.md) - Runtime configuration
- [Auto-Heal Guide](auto-heal.md) - Automatic recovery testing
- [Examples](../getting-started/examples.md) - More code samples

---

**Prev**: [Auto-Heal ←](auto-heal.md) | **Home**: [User Guide →](README.md)
