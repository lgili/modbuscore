# Interrupt-Driven Reception Support - Delta Spec

## ADDED Requirements

### Requirement: Byte Injection API
The protocol engine SHALL provide an API for injecting bytes from ISR context.

#### Scenario: Inject single byte
- **GIVEN** engine is initialized
- **WHEN** `mbc_engine_inject_byte(engine, byte)` is called from ISR
- **THEN** function SHALL return MBC_STATUS_OK
- **AND** byte SHALL be appended to RX buffer
- **AND** function SHALL complete in < 10 microseconds (typical MCU)

#### Scenario: Buffer overflow protection
- **GIVEN** RX buffer is full (260 bytes)
- **WHEN** `mbc_engine_inject_byte()` is called
- **THEN** function SHALL return MBC_STATUS_BUFFER_FULL
- **AND** byte SHALL be discarded
- **AND** engine state SHALL remain valid

### Requirement: Buffer Injection API
The protocol engine SHALL provide an API for injecting buffers from DMA completion.

#### Scenario: Inject buffer from DMA
- **GIVEN** DMA completion with 10 bytes received
- **WHEN** `mbc_engine_inject_buffer(engine, buffer, 10)` is called
- **THEN** function SHALL return MBC_STATUS_OK
- **AND** all 10 bytes SHALL be appended to RX buffer
- **AND** function SHALL be ISR-safe

#### Scenario: Partial buffer injection
- **GIVEN** RX buffer has space for 5 bytes
- **AND** attempting to inject 10 bytes
- **WHEN** `mbc_engine_inject_buffer()` is called
- **THEN** function SHALL inject 5 bytes
- **AND** return MBC_STATUS_PARTIAL
- **AND** indicate how many bytes were accepted

### Requirement: ISR Safety
Injection functions SHALL be safe to call from interrupt context.

#### Scenario: No dynamic allocation
- **GIVEN** injection function implementation
- **WHEN** static analysis is performed
- **THEN** no malloc/free calls SHALL be present
- **AND** no blocking operations SHALL be present
- **AND** all operations SHALL be bounded time

#### Scenario: Reentrancy safety
- **GIVEN** injection function is being called
- **WHEN** another interrupt attempts injection
- **THEN** behavior SHALL be well-defined
- **AND** data SHALL NOT be corrupted
- **AND** documentation SHALL specify reentrancy guarantees

**Reentrancy options:**
1. NOT reentrant - User must disable interrupts
2. Reentrant via atomic operations
3. Reentrant via critical sections (user-provided)

### Requirement: Frame Detection
The engine SHALL detect frame completion when bytes are injected.

#### Scenario: Complete frame injected byte-by-byte
- **GIVEN** engine receiving bytes via injection
- **WHEN** final byte of valid frame is injected
- **THEN** engine SHALL detect frame completion
- **AND** pdu_ready flag SHALL be set
- **AND** frame SHALL be available for processing

#### Scenario: Complete frame injected as buffer
- **GIVEN** complete Modbus frame in buffer
- **WHEN** `mbc_engine_inject_buffer()` is called with entire frame
- **THEN** engine SHALL detect frame completion
- **AND** frame SHALL be parsed immediately

### Requirement: Timeout Handling with Interrupts
The engine SHALL support frame timeout detection in interrupt-driven mode.

#### Scenario: Timeout via polling
- **GIVEN** partial frame received via injection
- **WHEN** `mbc_engine_step()` is called after timeout period
- **THEN** partial frame SHALL be discarded
- **AND** engine SHALL return to idle

#### Scenario: Timeout via timer ISR
- **GIVEN** user implements timer ISR for frame timeout
- **AND** partial frame is in buffer
- **WHEN** timer ISR calls timeout notification
- **THEN** engine SHALL discard partial frame
- **AND** engine SHALL return to idle

**Note:** Timer ISR integration is user responsibility, but library shall provide hooks.

### Requirement: Performance
Byte injection SHALL have minimal overhead for MCU applications.

#### Scenario: Injection latency
- **GIVEN** byte injection function
- **WHEN** called from ISR on typical MCU (e.g., 48MHz ARM Cortex-M0)
- **THEN** execution time SHALL be < 10 microseconds
- **AND** execution time SHALL be deterministic (no conditionals on data)

#### Scenario: CPU usage vs polling
- **GIVEN** interrupt-driven reception
- **WHEN** receiving 100 Modbus frames
- **THEN** CPU usage SHALL be < 50% of polling approach
- **AND** MCU SHALL be able to sleep between interrupts

### Requirement: Buffer Management
The engine SHALL manage RX buffer safely for interrupt usage.

#### Scenario: No buffer corruption
- **GIVEN** bytes being injected from ISR
- **AND** main loop processing frames
- **WHEN** concurrent access occurs
- **THEN** no buffer corruption SHALL occur
- **AND** data integrity SHALL be maintained

**Implementation note:** Read/write pointers or length tracking with atomic updates.

### Requirement: Example Integration
Examples SHALL demonstrate interrupt-driven patterns for common MCUs.

#### Scenario: UART RX ISR example
- **GIVEN** examples/renesas_rl78_isr/
- **WHEN** developer reviews example
- **THEN** example SHALL show UART RX ISR handler
- **AND** example SHALL call `mbc_engine_inject_byte()`
- **AND** example SHALL show critical section if needed
- **AND** example SHALL include complete ISR code

#### Scenario: DMA completion example
- **GIVEN** examples/stm32_dma/
- **WHEN** developer reviews example
- **THEN** example SHALL show DMA completion ISR
- **AND** example SHALL call `mbc_engine_inject_buffer()`
- **AND** example SHALL show circular buffer management
- **AND** example SHALL document timing

#### Scenario: Timer ISR timeout example
- **GIVEN** frame timeout example
- **WHEN** developer reviews example
- **THEN** example SHALL show timer ISR implementation
- **AND** example SHALL show how to reset timer on byte received
- **AND** example SHALL show timeout handling

### Requirement: Documentation
The library SHALL document ISR integration requirements clearly.

#### Scenario: ISR safety documentation
- **GIVEN** docs/advanced/interrupt-driven-rx.md
- **WHEN** developer reads documentation
- **THEN** ISR safety guarantees SHALL be documented
- **AND** timing requirements SHALL be specified
- **AND** reentrancy requirements SHALL be explained
- **AND** critical section requirements SHALL be documented

#### Scenario: Timing requirements
- **GIVEN** interrupt-driven documentation
- **WHEN** developer configures system
- **THEN** guide SHALL specify maximum ISR execution time
- **AND** guide SHALL explain ISR priority recommendations
- **AND** guide SHALL show how to calculate worst-case timing

### Requirement: Concurrent Processing
The engine SHALL support concurrent injection and processing.

#### Scenario: Process while receiving
- **GIVEN** frame being processed in main loop
- **WHEN** new bytes arrive via ISR injection
- **THEN** new bytes SHALL buffer correctly
- **AND** current frame processing SHALL not be corrupted
- **AND** next frame SHALL be available after current completes

### Requirement: Error Recovery
The engine SHALL recover from injection errors gracefully.

#### Scenario: Overflow recovery
- **GIVEN** buffer overflow occurred
- **WHEN** space becomes available
- **THEN** engine SHALL accept new bytes
- **AND** partial/corrupted frame SHALL be discarded
- **AND** normal operation SHALL resume

#### Scenario: ISR flood protection
- **GIVEN** very high incoming byte rate
- **WHEN** injection rate exceeds processing rate
- **THEN** engine SHALL not crash or hang
- **AND** oldest partial frames MAY be discarded
- **AND** system SHALL remain responsive

## MODIFIED Requirements

None - This is a new feature addition.

## REMOVED Requirements

None.

## RENAMED Requirements

None.
