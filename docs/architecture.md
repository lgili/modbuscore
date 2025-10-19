---
layout: default
title: Architecture Overview
nav_order: 2
---

# Architecture Overview
{: .no_toc }

Understanding the ModbusCore v1.0 layered architecture
{: .fs-6 .fw-300 }

## Table of Contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Design Philosophy

ModbusCore v1.0 is built on three core principles:

1. **Separation of Concerns** – Each layer has a single, well-defined responsibility
2. **Dependency Injection** – Components receive their dependencies, not hardcoded
3. **Testability** – Every layer can be tested in isolation with mocks

---

## Layer Architecture

```
┌─────────────────────────────────────────────────────┐
│                  Application Layer                   │
│        (Your code using ModbusCore API)             │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│               Protocol Engine (FSM)                  │
│  • State machine (IDLE, SENDING, RECEIVING, etc.)   │
│  • Request/response handling                        │
│  • Timeout management                               │
│  • Event notifications                              │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│                 Framing Layer                        │
│  • MBAP (TCP): 7-byte header + PDU                  │
│  • RTU: Unit ID + PDU + CRC (Phase 5)               │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│                  PDU Layer                           │
│  • Function code encoding/decoding                  │
│  • Request/response builders                        │
│  • Exception handling                               │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│                 Runtime Layer                        │
│  • Dependency container (DI)                        │
│  • Transport interface                              │
│  • Clock, allocator, logger, diagnostics interfaces│
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│               Transport Layer                        │
│  • POSIX TCP (Linux/macOS)                          │
│  • Winsock TCP (Windows)                            │
│  • POSIX RTU (serial, Phase 5)                      │
│  • Mock (testing)                                   │
│  • Custom implementations                           │
└─────────────────────────────────────────────────────┘
```

---

## Component Responsibilities

### 1. Transport Layer

**Purpose:** Abstract physical communication (TCP sockets, serial ports, etc.)

**Interface:**
```c
typedef struct mbc_transport_iface {
    void *ctx;
    mbc_status_t (*send)(void *ctx, const uint8_t *buffer, size_t length, ...);
    mbc_status_t (*receive)(void *ctx, uint8_t *buffer, size_t capacity, ...);
    uint64_t (*now)(void *ctx);
    void (*yield)(void *ctx);
} mbc_transport_iface_t;
```

**Implementations:**
- `posix_tcp.c` – Non-blocking TCP sockets (Linux/macOS)
- `winsock_tcp.c` – Non-blocking TCP sockets (Windows)
- `mock.c` – In-memory transport for testing
- *Future:* RTU serial, custom transports

---

### 2. Runtime Layer

**Purpose:** Dependency injection container

**Features:**
- Builder pattern for safe configuration
- Default implementations for optional dependencies
- Validation of required dependencies

**Example:**
```c
mbc_runtime_builder_t builder;
mbc_runtime_builder_init(&builder);
mbc_runtime_builder_with_transport(&builder, &transport);  // Required
// Clock, allocator, logger, diagnostics auto-filled with defaults

mbc_runtime_t runtime;
mbc_runtime_builder_build(&builder, &runtime);
```

**Dependencies Managed:**
- **Transport** (required) – Physical I/O
- **Clock** (optional, defaults to system time)
- **Allocator** (optional, defaults to malloc/free)
- **Logger** (optional, defaults to no-op)
- **Diagnostics sink** (optional, defaults to no-op trace)

---

### 3. PDU Layer

**Purpose:** Encode/decode Modbus Protocol Data Units

**Supported Function Codes:**
- **FC03** – Read Holding Registers
- **FC06** – Write Single Register
- **FC16** – Write Multiple Registers
- **Exceptions** – Error responses

**API Examples:**
```c
// Build FC03 request
mbc_pdu_t pdu;
mbc_pdu_build_read_holding_request(&pdu, unit_id, address, quantity);

// Parse FC03 response
const uint8_t *data;
size_t count;
mbc_pdu_parse_read_holding_response(&pdu, &data, &count);

// Parse exception
uint8_t original_fc, exception_code;
mbc_pdu_parse_exception(&pdu, &original_fc, &exception_code);
```

---

### 4. Framing Layer

**Purpose:** Add/remove transport-specific headers

#### MBAP (Modbus TCP)

7-byte header:
```
┌────────────┬────────────┬────────┬─────────┬─────────────┐
│ Trans ID   │ Protocol   │ Length │ Unit ID │ PDU         │
│ (2 bytes)  │ ID (0x00)  │ (2)    │ (1)     │ (variable)  │
└────────────┴────────────┴────────┴─────────┴─────────────┘
```

**API:**
```c
// Encode MBAP frame
mbc_mbap_header_t header = {
    .transaction_id = 1,
    .protocol_id = 0,
    .unit_id = 1
};
mbc_mbap_encode(&header, pdu_data, pdu_length, out_buffer, ...);

// Decode MBAP frame
mbc_mbap_decode(in_buffer, in_length, &header, &pdu_data, &pdu_length);

// Determine expected frame length (for incremental reception)
size_t expected = mbc_mbap_expected_length(partial_buffer, partial_length);
```

#### RTU (Modbus Serial) – Coming in Phase 5

Frame format:
```
┌─────────┬────────┬─────────────┬────────┐
│ Unit ID │ FC     │ Data        │ CRC16  │
│ (1)     │ (1)    │ (variable)  │ (2)    │
└─────────┴────────┴─────────────┴────────┘
```

---

### 5. Protocol Engine (FSM)

**Purpose:** Manage request/response state machine

**States:**
```c
typedef enum mbc_engine_state {
    MBC_ENGINE_STATE_IDLE,          // Ready for new operation
    MBC_ENGINE_STATE_RECEIVING,     // Receiving data
    MBC_ENGINE_STATE_SENDING,       // Sending data
    MBC_ENGINE_STATE_WAIT_RESPONSE  // Client waiting for response
} mbc_engine_state_t;
```

**Key Operations:**
```c
// Initialize engine
mbc_engine_config_t config = {
    .runtime = &runtime,
    .role = MBC_ENGINE_ROLE_CLIENT,
    .framing = MBC_FRAMING_TCP,
    .response_timeout_ms = 3000,
};
mbc_engine_init(&engine, &config);

// Submit request (transitions to SENDING → WAIT_RESPONSE)
mbc_engine_submit_request(&engine, buffer, length);

// Process incoming data (call periodically)
mbc_engine_step(&engine, budget);

// Take decoded PDU when ready
mbc_pdu_t response;
if (mbc_engine_take_pdu(&engine, &response)) {
    // Handle response
}
```

**Timeout Handling:**
- Client mode: automatic timeout detection
- Returns `MBC_STATUS_TIMEOUT` after configured period
- Resets automatically to IDLE state

---

## Data Flow Example

### Client Reading Holding Registers (FC03)

```
┌─────────────┐
│ Application │
└──────┬──────┘
       │ 1. Build PDU
       ↓
┌─────────────┐
│  PDU Layer  │  mbc_pdu_build_read_holding_request(...)
└──────┬──────┘
       │ 2. Wrap with MBAP
       ↓
┌─────────────┐
│ MBAP Layer  │  mbc_mbap_encode(...)
└──────┬──────┘
       │ 3. Submit to engine
       ↓
┌─────────────┐
│   Engine    │  mbc_engine_submit_request(...)
└──────┬──────┘
       │ 4. Send via transport
       ↓
┌─────────────┐
│  Transport  │  transport.send(...)
└──────┬──────┘
       │
       ↓
   ╔═══════════════╗
   ║  TCP Socket   ║
   ╚═══════════════╝

   ═══════ Response ═══════

   ╔═══════════════╗
   ║  TCP Socket   ║
   ╚═══════════════╝
       │
       ↓
┌─────────────┐
│  Transport  │  transport.receive(...)
└──────┬──────┘
       │ 5. Poll for data
       ↓
┌─────────────┐
│   Engine    │  mbc_engine_step(...)
└──────┬──────┘
       │ 6. Decode MBAP
       ↓
┌─────────────┐
│ MBAP Layer  │  mbc_mbap_decode(...)
└──────┬──────┘
       │ 7. Extract PDU
       ↓
┌─────────────┐
│  PDU Layer  │  mbc_pdu_parse_read_holding_response(...)
└──────┬──────┘
       │ 8. Return data
       ↓
┌─────────────┐
│ Application │
└─────────────┘
```

---

## Design Patterns Used

### 1. Dependency Injection

**Before (v2.x):**
```c
// Hardcoded dependencies
mb_client_t *client = mb_tcp_connect("192.168.1.10", 502);
```

**After (v1.0):**
```c
// Injected dependencies
mbc_transport_iface_t transport = create_tcp_transport(...);
mbc_runtime_builder_with_transport(&builder, &transport);
mbc_runtime_t runtime;
mbc_runtime_builder_build(&builder, &runtime);
```

**Benefits:**
- Easy to mock for testing
- Swap implementations without code changes
- Clear dependency graph

### 2. Builder Pattern

**Purpose:** Safe, incremental configuration

```c
mbc_runtime_builder_t builder;
mbc_runtime_builder_init(&builder);
mbc_runtime_builder_with_transport(&builder, &transport);  // Required
mbc_runtime_builder_with_clock(&builder, &custom_clock);   // Optional
// ... more optional configurations

mbc_runtime_t runtime;
status = mbc_runtime_builder_build(&builder, &runtime);  // Validates & fills defaults
```

### 3. State Machine (FSM)

**Engine States:**
```
     ┌─────┐
     │IDLE │ ◄────────────────────┐
     └──┬──┘                      │
        │                         │
        │ submit_request()        │
        ↓                         │
   ┌─────────┐                   │
   │SENDING  │                   │ timeout or
   └────┬────┘                   │ response received
        │                        │
        │ (client mode)          │
        ↓                        │
┌───────────────┐                │
│WAIT_RESPONSE  ├────────────────┘
└───────────────┘
        ↑
        │ (server mode)
        │
   ┌────┴────┐
   │RECEIVING│
   └─────────┘
```

### 4. Interface Segregation

Each dependency has a focused interface:
- **Transport** – Only send/receive/now/yield
- **Clock** – Only now_ms()
- **Allocator** – Only alloc/free()
- **Logger** – Only write()

---

## Testing Strategy

### Unit Tests

Each layer tested in isolation with mocks:

```c
// Mock transport for testing protocol engine
mbc_transport_iface_t mock_transport = {
    .ctx = &mock_ctx,
    .send = mock_send,
    .receive = mock_receive,
    .now = mock_now,
    .yield = NULL
};
```

### Integration Tests

Full stack tested against real servers:

```bash
# Start Python Modbus server
python3 tests/simple_tcp_server.py

# Run integration test
./build/tests/example_tcp_client_fc03
```

---

## Performance Considerations

### Non-Blocking I/O

All transport operations are **non-blocking**:
- Returns immediately if no data available
- Integrates with cooperative multitasking
- No threads or blocking calls

### Zero-Copy Where Possible

```c
// PDU parse returns pointer into existing buffer (no copy)
const uint8_t *data;
mbc_pdu_parse_read_holding_response(&pdu, &data, &count);
```

### Incremental Reception

```c
// Process small chunks (e.g., 10 bytes at a time)
for (int i = 0; i < 100; ++i) {
    mbc_engine_step(&engine, 10);  // Budget = 10 bytes
    // Check if PDU ready...
}
```

---

## Next Steps

- [**Dependency Injection Guide**](dependency-injection.md)
- [**Transport Layer Details**](transports.md)
- [**Protocol Engine Deep Dive**](protocol-engine.md)
- [**API Reference**](api/)
