# Input Register Support (FC04) - Delta Spec

## ADDED Requirements

### Requirement: FC04 Request Building
The PDU module SHALL support building FC04 (Read Input Registers) request PDUs.

#### Scenario: Build valid FC04 request
- **GIVEN** valid parameters (unit_id=1, address=0x0100, quantity=10)
- **WHEN** `mbc_pdu_build_read_input_request()` is called
- **THEN** PDU SHALL be built successfully
- **AND** function code SHALL be 0x04
- **AND** payload SHALL contain address (big-endian)
- **AND** payload SHALL contain quantity (big-endian)
- **AND** payload length SHALL be 4 bytes

#### Scenario: Reject invalid quantity
- **GIVEN** quantity = 0 or quantity > 125
- **WHEN** `mbc_pdu_build_read_input_request()` is called
- **THEN** function SHALL return MBC_STATUS_INVALID_ARGUMENT
- **AND** PDU SHALL remain unchanged

### Requirement: FC04 Response Parsing
The PDU module SHALL support parsing FC04 response PDUs.

#### Scenario: Parse valid FC04 response
- **GIVEN** valid FC04 response PDU with byte_count and register data
- **WHEN** `mbc_pdu_parse_read_input_response()` is called
- **THEN** function SHALL return MBC_STATUS_OK
- **AND** out_data SHALL point to register data in PDU
- **AND** out_registers SHALL contain correct register count
- **AND** register count SHALL match byte_count / 2

#### Scenario: Reject malformed response
- **GIVEN** FC04 response with byte_count > payload_length
- **WHEN** `mbc_pdu_parse_read_input_response()` is called
- **THEN** function SHALL return MBC_STATUS_MALFORMED
- **AND** out parameters SHALL remain unchanged

### Requirement: Input Register Management API
The server runtime SHALL provide API for registering input registers.

#### Scenario: Register single input register
- **GIVEN** valid address and variable pointer
- **WHEN** `mbc_server_set_input_register()` is called
- **THEN** function SHALL return MBC_STATUS_OK
- **AND** register SHALL be added to input register list
- **AND** register count SHALL increment by 1

#### Scenario: Register input register array
- **GIVEN** starting address, length, and array pointer
- **WHEN** `mbc_server_set_input_register_array()` is called
- **THEN** function SHALL return MBC_STATUS_OK
- **AND** all registers SHALL be added sequentially
- **AND** register count SHALL increment by length

#### Scenario: Prevent duplicate registration
- **GIVEN** address 0x0100 already registered
- **WHEN** attempting to register 0x0100 again
- **THEN** function SHALL return MBC_STATUS_ALREADY_EXISTS
- **AND** original registration SHALL remain unchanged

### Requirement: Input Register Read Operations
The server SHALL handle FC04 requests and return input register values.

#### Scenario: Read single input register
- **GIVEN** FC04 request for address=0x0100, quantity=1
- **AND** register 0x0100 contains value 0x1234
- **WHEN** request is processed
- **THEN** response SHALL contain function code 0x04
- **AND** byte count SHALL be 2
- **AND** register value SHALL be 0x1234 (big-endian)

#### Scenario: Read multiple input registers
- **GIVEN** FC04 request for address=0x0100, quantity=5
- **AND** registers 0x0100-0x0104 are registered
- **WHEN** request is processed
- **THEN** response SHALL contain function code 0x04
- **AND** byte count SHALL be 10 (5 * 2 bytes)
- **AND** all 5 register values SHALL be in response (big-endian order)

#### Scenario: Handle unregistered address
- **GIVEN** FC04 request for address=0x0200
- **AND** address 0x0200 is not registered
- **WHEN** request is processed
- **THEN** exception response SHALL be sent
- **AND** exception code SHALL be 0x02 (Illegal Data Address)

### Requirement: Address Space Separation
Input registers SHALL occupy separate address space from holding registers.

#### Scenario: Same address different register types
- **GIVEN** holding register at address 0x0100 with value 100
- **AND** input register at address 0x0100 with value 200
- **WHEN** FC03 request reads address 0x0100
- **THEN** response SHALL return 100
- **WHEN** FC04 request reads address 0x0100
- **THEN** response SHALL return 200

### Requirement: Read Callback Support
Input registers SHALL support read callbacks for dynamic values.

#### Scenario: Callback returns current value
- **GIVEN** input register at 0x0100 with callback
- **AND** callback returns current sensor value
- **WHEN** FC04 request reads 0x0100
- **THEN** callback SHALL be invoked
- **AND** response SHALL contain callback return value

#### Scenario: NULL callback uses direct memory
- **GIVEN** input register at 0x0100 with NULL callback
- **AND** variable contains value 0x5678
- **WHEN** FC04 request reads 0x0100
- **THEN** response SHALL contain 0x5678
- **AND** no callback SHALL be invoked

### Requirement: Performance
Input register operations SHALL have minimal performance overhead.

#### Scenario: Read operation performance
- **GIVEN** 100 input registers registered
- **WHEN** FC04 request reads 10 consecutive registers
- **THEN** lookup time SHALL be O(log n) or better
- **AND** total processing time SHALL be < 1ms on typical MCU

### Requirement: Memory Efficiency
Input register storage SHALL be memory efficient for MCU constraints.

#### Scenario: Memory footprint per register
- **GIVEN** input register structure
- **WHEN** calculating memory usage
- **THEN** overhead SHALL be <= 16 bytes per register
- **AND** total memory SHALL be predictable at compile time

## MODIFIED Requirements

None - This is a new feature addition.

## REMOVED Requirements

None.

## RENAMED Requirements

None.
