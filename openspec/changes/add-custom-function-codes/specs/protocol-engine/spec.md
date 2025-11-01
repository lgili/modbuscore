# Custom Function Code Support - Delta Spec

## ADDED Requirements

### Requirement: Custom Function Code Handler Signature
The runtime SHALL define a standard signature for custom function code handlers.

#### Scenario: Handler signature
- **GIVEN** handler signature definition
- **THEN** handler SHALL receive context parameter (void*)
- **AND** handler SHALL receive unit ID (uint8_t)
- **AND** handler SHALL receive function code (uint8_t)
- **AND** handler SHALL receive request payload (const uint8_t*, size_t)
- **AND** handler SHALL receive response buffer (uint8_t*, size_t*)
- **AND** handler SHALL return status (success, exception, error)

**Signature:**
```c
typedef enum {
    MBC_CUSTOM_FC_OK = 0,           // Success, response built
    MBC_CUSTOM_FC_EXCEPTION = 1,    // Exception, code in response
    MBC_CUSTOM_FC_NO_RESPONSE = 2,  // Broadcast, no response
    MBC_CUSTOM_FC_ERROR = 3         // Internal error
} mbc_custom_fc_result_t;

typedef mbc_custom_fc_result_t (*mbc_custom_fc_handler_t)(
    void* ctx,
    uint8_t unit_id,
    uint8_t function_code,
    const uint8_t* request_payload,
    size_t request_length,
    uint8_t* response_payload,
    size_t* response_length,
    uint8_t* exception_code
);
```

#### Scenario: Handler invocation
- **GIVEN** custom FC handler registered for FC 0x41
- **WHEN** request with FC 0x41 is received
- **THEN** handler SHALL be invoked
- **AND** handler SHALL receive request data
- **AND** handler SHALL build response

### Requirement: Custom FC Registration API
The server SHALL provide API for registering custom function code handlers.

#### Scenario: Register custom function code
- **GIVEN** valid function code (0x01-0x7F, not standard)
- **AND** handler function
- **AND** optional context
- **WHEN** `mbc_server_register_custom_fc()` is called
- **THEN** function SHALL return MBC_STATUS_OK
- **AND** handler SHALL be stored
- **AND** future requests SHALL invoke handler

#### Scenario: Reject standard function codes
- **GIVEN** attempt to register standard FC (0x03, 0x06, etc.)
- **WHEN** `mbc_server_register_custom_fc()` is called
- **THEN** function SHALL return MBC_STATUS_INVALID_ARGUMENT
- **AND** handler SHALL NOT be registered

#### Scenario: Replace existing custom handler
- **GIVEN** custom FC 0x41 already registered
- **WHEN** registering new handler for FC 0x41
- **THEN** old handler SHALL be replaced
- **OR** function SHALL return error (implementation choice)

#### Scenario: Unregister custom function code
- **GIVEN** custom FC 0x41 registered
- **WHEN** `mbc_server_unregister_custom_fc(0x41)` is called
- **THEN** function SHALL return MBC_STATUS_OK
- **AND** handler SHALL be removed
- **AND** future FC 0x41 requests SHALL return exception

### Requirement: Custom FC Dispatch
The protocol engine SHALL dispatch custom function codes to registered handlers.

#### Scenario: Dispatch to custom handler
- **GIVEN** request with custom FC 0x42
- **AND** handler registered for FC 0x42
- **WHEN** request is processed
- **THEN** engine SHALL lookup handler
- **AND** engine SHALL invoke handler
- **AND** standard processing SHALL be bypassed

#### Scenario: No handler registered
- **GIVEN** request with FC 0x50
- **AND** no handler registered for FC 0x50
- **WHEN** request is processed
- **THEN** exception response SHALL be sent
- **AND** exception code SHALL be 0x01 (Illegal Function)

#### Scenario: Standard FC takes precedence
- **GIVEN** custom handler accidentally registered for standard FC
- **WHEN** standard FC request is received
- **THEN** standard processing SHALL occur
- **AND** custom handler SHALL NOT be invoked

### Requirement: Response Building
Custom handlers SHALL build responses in provided buffer.

#### Scenario: Build successful response
- **GIVEN** custom FC handler
- **AND** request processed successfully
- **WHEN** building response
- **THEN** handler SHALL write response data to buffer
- **AND** handler SHALL set response_length
- **AND** handler SHALL return MBC_CUSTOM_FC_OK

#### Scenario: Buffer overflow protection
- **GIVEN** response buffer size is 253 bytes
- **WHEN** handler attempts to write > 253 bytes
- **THEN** handler SHOULD detect overflow
- **AND** handler SHALL return error or truncate
- **AND** documentation SHALL warn about buffer size

#### Scenario: Empty response
- **GIVEN** custom FC that requires no response data
- **WHEN** handler builds response
- **THEN** handler SHALL set response_length = 0
- **AND** only function code SHALL be in response

### Requirement: Exception Handling
Custom handlers SHALL be able to return Modbus exceptions.

#### Scenario: Return exception
- **GIVEN** custom FC handler detects error
- **WHEN** handler processes request
- **THEN** handler SHALL set exception_code (0x01-0x04)
- **AND** handler SHALL return MBC_CUSTOM_FC_EXCEPTION
- **AND** engine SHALL build exception response (FC | 0x80)

#### Scenario: Valid exception codes
- **GIVEN** handler returning exception
- **WHEN** setting exception code
- **THEN** exception code SHALL be 0x01-0x04
- **OR** vendor-specific codes >= 0x80 (if supported)

### Requirement: Broadcast Handling
Custom handlers SHALL support broadcast requests.

#### Scenario: Broadcast flag passed to handler
- **GIVEN** broadcast request (unit ID = 0)
- **AND** custom FC handler
- **WHEN** handler is invoked
- **THEN** unit_id parameter SHALL be 0
- **AND** handler SHALL detect broadcast
- **AND** handler MAY suppress response

#### Scenario: Suppress broadcast response
- **GIVEN** handler detects broadcast
- **WHEN** handler returns MBC_CUSTOM_FC_NO_RESPONSE
- **THEN** no response SHALL be sent
- **AND** engine SHALL return to idle

#### Scenario: Broadcast with response (error)
- **GIVEN** handler returns MBC_CUSTOM_FC_OK for broadcast
- **WHEN** engine processes result
- **THEN** response SHALL be suppressed anyway
- **OR** documentation SHALL warn against this

### Requirement: Buffer Ownership and Safety
Buffer management SHALL be clearly documented and safe.

#### Scenario: Request buffer is read-only
- **GIVEN** handler receives request_payload
- **WHEN** handler accesses request data
- **THEN** request buffer SHALL be const
- **AND** handler SHALL NOT modify request
- **AND** buffer SHALL remain valid during handler execution

#### Scenario: Response buffer is writable
- **GIVEN** handler receives response_payload
- **WHEN** handler builds response
- **THEN** response buffer SHALL be writable
- **AND** buffer size SHALL be documented (253 bytes max)
- **AND** buffer SHALL be zeroed before handler call

#### Scenario: Buffer lifetime
- **GIVEN** handler completes execution
- **WHEN** returning from handler
- **THEN** request buffer SHALL become invalid
- **AND** handler SHALL NOT retain pointer
- **AND** response buffer ownership transfers to engine

### Requirement: Multiple Custom FC Support
The server SHALL support registration of multiple custom function codes.

#### Scenario: Register multiple handlers
- **GIVEN** handlers for FC 0x41, 0x42, 0x43
- **WHEN** all three are registered
- **THEN** all SHALL be stored
- **AND** each SHALL be invoked for respective FC
- **AND** no conflicts SHALL occur

#### Scenario: Handler storage efficiency
- **GIVEN** N custom function codes registered
- **WHEN** calculating memory usage
- **THEN** overhead SHALL be <= 16 bytes per handler
- **AND** lookup SHALL be O(1) or O(log N)

### Requirement: Context Parameter
Handlers SHALL receive user context for state access.

#### Scenario: Context contains application state
- **GIVEN** handler registered with context pointer
- **WHEN** handler is invoked
- **THEN** context SHALL be passed to handler
- **AND** handler SHALL have access to app state
- **AND** context SHALL be same pointer as registration

**Example use cases:**
- Device configuration
- State machine reference
- Hardware handles
- Statistics counters

### Requirement: Example Implementations
Examples SHALL demonstrate common custom FC patterns.

#### Scenario: Broadcast group command example
- **GIVEN** examples/custom_fc_broadcast/
- **WHEN** developer reviews example
- **THEN** example SHALL implement FC 0x41 (group control)
- **AND** example SHALL detect broadcast
- **AND** example SHALL suppress response
- **AND** example SHALL update group state

#### Scenario: Commissioning command example
- **GIVEN** examples/custom_fc_commissioning/
- **WHEN** developer reviews example
- **THEN** example SHALL implement FC 0x43 (auto-addressing)
- **AND** example SHALL read GPIO state
- **AND** example SHALL update device address
- **AND** example SHALL return success response

#### Scenario: Vendor diagnostic example
- **GIVEN** examples/custom_fc_diagnostics/
- **WHEN** developer reviews example
- **THEN** example SHALL implement vendor-specific FC
- **AND** example SHALL collect diagnostic data
- **AND** example SHALL build multi-byte response
- **AND** example SHALL handle exceptions

### Requirement: Thread Safety
Custom FC registration and invocation SHALL be thread-safe.

#### Scenario: Register from main thread
- **GIVEN** registration happens during initialization
- **WHEN** handler is registered
- **THEN** registration SHALL complete atomically
- **AND** handler SHALL be available immediately

#### Scenario: Invoke from ISR context
- **GIVEN** Modbus processing in ISR
- **AND** custom FC request received
- **WHEN** handler is invoked
- **THEN** handler SHALL execute in ISR context
- **AND** handler SHALL be ISR-safe (user responsibility)

### Requirement: Performance
Custom FC dispatch SHALL have minimal overhead.

#### Scenario: Dispatch overhead
- **GIVEN** custom FC handler registered
- **WHEN** custom FC request is processed
- **THEN** handler lookup SHALL be < 1 microsecond
- **AND** total overhead SHALL be < 10 microseconds
- **AND** performance SHALL be comparable to standard FC

## MODIFIED Requirements

None - This is a new feature addition.

## REMOVED Requirements

None.

## RENAMED Requirements

None.
