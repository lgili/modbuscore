# Register Callback System - Delta Spec

## ADDED Requirements

### Requirement: Read Callback Signature
The runtime SHALL define a standard signature for register read callbacks.

#### Scenario: Read callback signature
- **GIVEN** callback signature definition
- **THEN** callback SHALL accept context parameter (void*)
- **AND** callback SHALL accept register address (uint16_t)
- **AND** callback SHALL return register value (int16_t)
- **AND** callback MAY return error via special value or out parameter

**Signature:**
```c
typedef int16_t (*mbc_read_callback_t)(void* ctx, uint16_t address);
```

#### Scenario: Read callback invocation
- **GIVEN** register with read callback
- **WHEN** FC03 or FC04 read request
- **THEN** callback SHALL be invoked with context and address
- **AND** callback return value SHALL be sent in response

### Requirement: Write Callback Signature
The runtime SHALL define a standard signature for register write callbacks.

#### Scenario: Write callback signature
- **GIVEN** callback signature definition
- **THEN** callback SHALL accept context parameter (void*)
- **AND** callback SHALL accept register address (uint16_t)
- **AND** callback SHALL accept new value (int16_t)
- **AND** callback SHALL return accepted value (int16_t)
- **AND** callback MAY return different value (clamping/correction)

**Signature:**
```c
typedef int16_t (*mbc_write_callback_t)(void* ctx, uint16_t address, int16_t value);
```

#### Scenario: Write callback invocation
- **GIVEN** register with write callback
- **WHEN** FC06 or FC16 write request
- **THEN** callback SHALL be invoked with context, address, and value
- **AND** callback return value SHALL be stored
- **AND** response SHALL confirm write

### Requirement: Callback Registration API
The server SHALL support registering callbacks during register configuration.

#### Scenario: Register holding register with callbacks
- **GIVEN** valid address, variable pointer, callbacks, and context
- **WHEN** `mbc_server_set_holding_register()` is called
- **THEN** function SHALL return MBC_STATUS_OK
- **AND** register SHALL store read callback
- **AND** register SHALL store write callback
- **AND** register SHALL store context pointer

#### Scenario: Register input register with read callback
- **GIVEN** valid address, variable pointer, read callback, and context
- **WHEN** `mbc_server_set_input_register()` is called
- **THEN** function SHALL return MBC_STATUS_OK
- **AND** register SHALL store read callback
- **AND** register SHALL store context pointer

#### Scenario: Register without callbacks (NULL)
- **GIVEN** address and variable, but NULL callbacks
- **WHEN** register is set
- **THEN** function SHALL return MBC_STATUS_OK
- **AND** direct memory access SHALL be used

### Requirement: Read Callback Invocation
The server SHALL invoke read callbacks on read requests.

#### Scenario: Read with callback
- **GIVEN** holding register at 0x0100 with read callback
- **AND** callback returns dynamic value 0x5678
- **WHEN** FC03 read request for 0x0100
- **THEN** read callback SHALL be invoked
- **AND** response SHALL contain 0x5678
- **AND** variable memory SHALL NOT be accessed

#### Scenario: Read without callback
- **GIVEN** holding register at 0x0100 with NULL callback
- **AND** variable contains 0x1234
- **WHEN** FC03 read request for 0x0100
- **THEN** read callback SHALL NOT be invoked
- **AND** response SHALL contain 0x1234
- **AND** variable memory SHALL be accessed directly

### Requirement: Write Callback Invocation
The server SHALL invoke write callbacks on write requests.

#### Scenario: Write with callback accepts value
- **GIVEN** holding register at 0x0100 with write callback
- **AND** callback accepts and returns new value
- **WHEN** FC06 write 0xABCD to 0x0100
- **THEN** write callback SHALL be invoked with 0xABCD
- **AND** callback return value SHALL be stored
- **AND** response SHALL confirm write

#### Scenario: Write with callback clamps value
- **GIVEN** holding register with write callback
- **AND** callback clamps value to range [0, 1000]
- **WHEN** FC06 write 5000 to register
- **THEN** write callback SHALL be invoked with 5000
- **AND** callback SHALL return 1000 (clamped)
- **AND** variable SHALL contain 1000
- **AND** response SHALL confirm write

#### Scenario: Write without callback
- **GIVEN** holding register with NULL write callback
- **WHEN** FC06 write 0x9999 to register
- **THEN** write callback SHALL NOT be invoked
- **AND** variable SHALL be updated directly to 0x9999

### Requirement: Write Validation and Rejection
Write callbacks SHALL be able to reject invalid values.

#### Scenario: Callback rejects write
- **GIVEN** holding register with validation callback
- **AND** callback rejects values outside [MIN, MAX]
- **WHEN** FC06 write with out-of-range value
- **THEN** write callback SHALL be invoked
- **AND** callback SHALL return error indication
- **AND** variable SHALL NOT be updated
- **AND** exception response SHALL be sent with code 0x03 (Illegal Data Value)

**Implementation note:** Callback can return special value (e.g., INT16_MIN) to indicate rejection, or use extended signature with status return.

### Requirement: Read-Only Register Protection
The server SHALL enforce read-only flag independently of callbacks.

#### Scenario: Read-only register blocks writes
- **GIVEN** holding register marked as read-only
- **WHEN** FC06 write request
- **THEN** write SHALL be rejected before callback invocation
- **AND** exception response SHALL be sent with code 0x03 (Illegal Data Value)

#### Scenario: Read-only allows reads
- **GIVEN** holding register marked as read-only
- **WHEN** FC03 read request
- **THEN** read SHALL proceed normally
- **AND** read callback SHALL be invoked (if present)

### Requirement: Callback Context Parameter
The server SHALL pass user context to callbacks for state access.

#### Scenario: Context contains application state
- **GIVEN** register with callback and context pointing to app state
- **WHEN** callback is invoked
- **THEN** context parameter SHALL be passed to callback
- **AND** callback SHALL have access to app state
- **AND** callback MAY read from or modify app state

**Example use cases:**
- ADC handle
- Flash memory handle
- Configuration structure
- State machine context

### Requirement: Array Registration with Callbacks
The server SHALL support callback registration for register arrays.

#### Scenario: Register array with shared callback
- **GIVEN** array of 10 registers with single callback
- **WHEN** `mbc_server_set_holding_register_array()` is called
- **THEN** all registers SHALL share same callback
- **AND** callback receives address parameter to distinguish registers

#### Scenario: Register array with per-register callbacks
- **GIVEN** array registration API supports callback array
- **WHEN** array is registered
- **THEN** each register MAY have different callback
- **AND** callback index matches register index

### Requirement: Performance
Callback mechanism SHALL have minimal overhead when unused.

#### Scenario: NULL callback performance
- **GIVEN** register with NULL callbacks
- **WHEN** reading/writing register
- **THEN** overhead SHALL be single NULL check
- **AND** performance SHALL match direct memory access

#### Scenario: Callback invocation overhead
- **GIVEN** register with callbacks
- **WHEN** 1000 reads/writes executed
- **THEN** overhead SHALL be < 5% vs direct memory
- **AND** function call overhead SHALL dominate

### Requirement: Thread Safety
Callback invocation SHALL be safe for interrupt-driven implementations.

#### Scenario: Callback from ISR
- **GIVEN** Modbus processing in main loop
- **AND** register write triggers callback from ISR context
- **WHEN** callback is invoked
- **THEN** callback SHALL execute in same context as caller
- **AND** no additional interrupts SHALL be enabled
- **AND** callback SHALL complete within bounded time

**Documentation note:** User responsibility to ensure callback is reentrant if needed.

### Requirement: Error Handling
The system SHALL handle callback errors gracefully.

#### Scenario: Callback crashes or hangs
- **GIVEN** callback with bug (infinite loop, null deref)
- **WHEN** callback is invoked
- **THEN** behavior is user responsibility
- **AND** documentation SHALL warn about callback quality

**Mitigation:** Debug builds MAY add timeout or watchdog, but not required in production.

## MODIFIED Requirements

### Requirement: Holding Register Registration (MODIFIED)
The server API for holding register registration SHALL support callbacks.

**Previous signature:**
```c
mbc_status_t mbc_server_set_holding_register(
    mbc_runtime_t* runtime,
    uint16_t address,
    int16_t* variable
);
```

**New signature:**
```c
mbc_status_t mbc_server_set_holding_register(
    mbc_runtime_t* runtime,
    uint16_t address,
    int16_t* variable,
    bool read_only,
    mbc_read_callback_t read_cb,
    mbc_write_callback_t write_cb,
    void* callback_ctx
);
```

#### Scenario: Backward compatibility
- **GIVEN** existing code calling old API
- **WHEN** migrating to new API
- **THEN** passing NULL for callbacks SHALL work
- **AND** behavior SHALL match old direct memory access

### Requirement: Input Register Registration (MODIFIED)
The server API for input register registration SHALL support read callbacks.

**New signature:**
```c
mbc_status_t mbc_server_set_input_register(
    mbc_runtime_t* runtime,
    uint16_t address,
    int16_t* variable,
    mbc_read_callback_t read_cb,
    void* callback_ctx
);
```

#### Scenario: Register input with callback
- **GIVEN** valid address, variable, and read callback
- **WHEN** `mbc_server_set_input_register()` is called
- **THEN** function SHALL return MBC_STATUS_OK
- **AND** register SHALL store read callback
- **AND** register SHALL store context pointer

## REMOVED Requirements

None.

## RENAMED Requirements

None.
