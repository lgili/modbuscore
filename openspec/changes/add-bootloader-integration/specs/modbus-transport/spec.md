# Modbus Transport Bootloader Integration - Delta Spec

## ADDED Requirements

### Requirement: Bootloader Frame Interception
The transport layer SHALL provide a callback mechanism to intercept and process custom non-Modbus frames before standard Modbus processing.

#### Scenario: Bootloader callback not registered
- **GIVEN** transport is initialized with NULL bootloader callback
- **WHEN** a Modbus frame is received
- **THEN** normal Modbus processing SHALL occur without overhead

#### Scenario: Bootloader callback registered but frame not handled
- **GIVEN** transport has bootloader callback registered
- **WHEN** callback returns MODBUS_BOOTLOADER_NOT_HANDLED
- **THEN** frame SHALL continue through normal Modbus processing

#### Scenario: Bootloader frame successfully handled
- **GIVEN** transport has bootloader callback registered
- **WHEN** callback returns MODBUS_BOOTLOADER_HANDLED
- **THEN** normal Modbus processing SHALL be bypassed
- **AND** custom response SHALL be transmitted
- **AND** FSM SHALL transition to SENDING state

#### Scenario: Bootloader processing error
- **GIVEN** transport has bootloader callback registered
- **WHEN** callback returns MODBUS_BOOTLOADER_ERROR
- **THEN** FSM SHALL transition to ERROR state
- **AND** error recovery SHALL be initiated

### Requirement: Bootloader Callback Signature
The bootloader callback SHALL have the following signature and behavior:

```c
typedef uint8_t (*modbus_bootloader_callback_t)(
    const uint8_t *rx_buffer,
    uint16_t rx_length,
    uint8_t *tx_buffer,
    uint16_t *tx_length
);
```

#### Scenario: Callback receives valid parameters
- **GIVEN** a Modbus frame has been received
- **WHEN** bootloader callback is invoked
- **THEN** rx_buffer SHALL point to complete received frame
- **AND** rx_length SHALL be the total frame length
- **AND** tx_buffer SHALL be writable response buffer
- **AND** tx_length SHALL be output parameter for response length

#### Scenario: Callback returns appropriate status
- **GIVEN** bootloader callback is processing a frame
- **WHEN** frame is not a bootloader command
- **THEN** callback SHALL return MODBUS_BOOTLOADER_NOT_HANDLED
- **WHEN** frame is processed successfully
- **THEN** callback SHALL return MODBUS_BOOTLOADER_HANDLED
- **AND** tx_length SHALL contain valid response length
- **WHEN** processing error occurs
- **THEN** callback SHALL return MODBUS_BOOTLOADER_ERROR

### Requirement: FSM Bootloader Event
The FSM SHALL support a MODBUS_EVENT_BOOTLOADER event for custom frame handling.

#### Scenario: Bootloader event triggers state transition
- **GIVEN** FSM is in PROCESSING state
- **WHEN** MODBUS_EVENT_BOOTLOADER is triggered
- **THEN** FSM SHALL transition directly to SENDING state
- **AND** standard validation steps SHALL be skipped
- **AND** CRC calculation for response SHALL proceed normally

### Requirement: Thread Safety
Bootloader callback invocation SHALL be thread-safe for interrupt-driven implementations.

#### Scenario: Callback invoked from interrupt context
- **GIVEN** Modbus RX interrupt is active
- **WHEN** bootloader callback is invoked
- **THEN** callback SHALL execute in same interrupt context
- **AND** no new interrupts SHALL be enabled during callback
- **AND** callback SHALL complete within documented time limits

### Requirement: Buffer Management
The library SHALL provide safe buffer access to bootloader callbacks.

#### Scenario: Callback writes within buffer bounds
- **GIVEN** tx_buffer has known size MAX_TX_BUFFER
- **WHEN** callback writes response
- **THEN** library SHALL document maximum safe write size
- **AND** callback SHOULD validate bounds before writing
- **AND** response length SHALL NOT exceed buffer capacity

#### Scenario: Callback exceeds buffer bounds
- **GIVEN** tx_buffer has size MAX_TX_BUFFER
- **WHEN** callback writes beyond buffer bounds
- **THEN** behavior is undefined (user responsibility)
- **AND** debug builds MAY detect overflow

### Requirement: Performance Impact
Bootloader callback support SHALL have minimal performance impact when disabled.

#### Scenario: NULL callback performance
- **GIVEN** transport has NULL bootloader callback
- **WHEN** 1000 Modbus frames are processed
- **THEN** throughput SHALL be >= 99% of baseline (no callback check)
- **AND** latency increase SHALL be <= 1% of baseline

### Requirement: Documentation
Bootloader integration SHALL be fully documented with examples.

#### Scenario: Integration guide available
- **GIVEN** developer wants to add bootloader support
- **WHEN** reading docs/advanced/bootloader-integration.md
- **THEN** guide SHALL explain callback registration
- **AND** guide SHALL provide protocol design recommendations
- **AND** guide SHALL show complete working example
- **AND** guide SHALL document timing constraints
- **AND** guide SHALL explain thread safety considerations

#### Scenario: Reference example available
- **GIVEN** developer needs bootloader implementation
- **WHEN** reviewing examples/renesas_rl78_bootloader/
- **THEN** example SHALL demonstrate two-stage bootloader protocol
- **AND** example SHALL show CRC calculation
- **AND** example SHALL show flash programming interface
- **AND** example SHALL include README with wiring diagram

## MODIFIED Requirements

None - This is a pure addition to existing transport capabilities.

## REMOVED Requirements

None.

## RENAMED Requirements

None.
