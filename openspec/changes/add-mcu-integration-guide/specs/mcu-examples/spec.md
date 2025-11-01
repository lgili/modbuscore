# MCU Integration Guide and Examples - Delta Spec

## ADDED Requirements

### Requirement: MCU Integration Guide
The library SHALL provide comprehensive documentation for MCU integration.

#### Scenario: Step-by-step integration guide
- **GIVEN** developer wants to port library to new MCU
- **WHEN** reading docs/getting-started/mcu-integration.md
- **THEN** guide SHALL list all required steps
- **AND** guide SHALL explain transport layer implementation
- **AND** guide SHALL provide code templates
- **AND** guide SHALL reference complete examples

#### Scenario: Transport layer explained
- **GIVEN** developer reading integration guide
- **WHEN** reviewing transport layer section
- **THEN** guide SHALL explain all callback functions
- **AND** guide SHALL show minimal implementation
- **AND** guide SHALL explain optional callbacks
- **AND** guide SHALL provide timing requirements for each callback

#### Scenario: UART configuration explained
- **GIVEN** developer configuring UART
- **WHEN** reading UART configuration section
- **THEN** guide SHALL explain frame format (8N1, 8E1, etc.)
- **AND** guide SHALL show baud rate calculation
- **AND** guide SHALL explain interrupt vs polling trade-offs
- **AND** guide SHALL provide register configuration examples

### Requirement: Renesas RL78 Example
The library SHALL provide production-ready Renesas RL78 example.

#### Scenario: Complete RL78 implementation
- **GIVEN** examples/renesas_rl78/ directory
- **WHEN** developer reviews example
- **THEN** example SHALL include complete UART driver
- **AND** example SHALL show interrupt-driven RX/TX
- **AND** example SHALL implement all transport callbacks
- **AND** example SHALL demonstrate RS485 DE/RE control
- **AND** example SHALL include build configuration

#### Scenario: RL78 README documentation
- **GIVEN** RL78 example
- **WHEN** reading README
- **THEN** README SHALL include wiring diagram
- **AND** README SHALL list required tools
- **AND** README SHALL show build steps
- **AND** README SHALL explain clock configuration
- **AND** README SHALL provide troubleshooting tips

### Requirement: Second MCU Family Example
The library SHALL provide example for at least one additional MCU family.

#### Scenario: STM32 or AVR example available
- **GIVEN** developer targeting STM32 or AVR
- **WHEN** reviewing examples directory
- **THEN** complete working example SHALL exist
- **AND** example SHALL demonstrate different patterns than RL78
- **AND** example SHALL use standard HAL or SDK
- **AND** example SHALL compile with free toolchain

### Requirement: Template Code
The library SHALL provide reusable template code for common integration tasks.

#### Scenario: Transport layer template
- **GIVEN** examples/templates/transport_template.c
- **WHEN** developer copies template
- **THEN** template SHALL include all callback stubs
- **AND** template SHALL have TODO markers for customization
- **AND** template SHALL compile without modification (with warnings)
- **AND** template SHALL document each section

#### Scenario: Interrupt handler template
- **GIVEN** examples/templates/uart_isr_template.c
- **WHEN** developer copies template
- **THEN** template SHALL show RX interrupt pattern
- **AND** template SHALL show TX interrupt pattern
- **AND** template SHALL demonstrate buffer management
- **AND** template SHALL include timing comments

### Requirement: RS485 Control Patterns
Documentation SHALL explain both RS485 control variations.

#### Scenario: DE/RE pin control documented
- **GIVEN** developer using RS485 with control pin
- **WHEN** reading RS485 section
- **THEN** guide SHALL explain DE/RE pin purpose
- **AND** guide SHALL show when to toggle pin
- **AND** guide SHALL provide timing diagrams
- **AND** guide SHALL show example code

#### Scenario: Pure TX/RX documented
- **GIVEN** developer using RS485 without control pin
- **WHEN** reading RS485 section
- **THEN** guide SHALL explain when this is applicable
- **AND** guide SHALL show configuration differences
- **AND** guide SHALL explain trade-offs
- **AND** guide SHALL provide example code

### Requirement: Interrupt-Driven Pattern
Documentation SHALL explain interrupt-driven implementation.

#### Scenario: RX interrupt explained
- **GIVEN** developer implementing RX interrupt
- **WHEN** reading interrupt section
- **THEN** guide SHALL explain ISR best practices
- **AND** guide SHALL show buffer management
- **AND** guide SHALL explain FSM integration
- **AND** guide SHALL provide complete ISR example

#### Scenario: TX interrupt explained
- **GIVEN** developer implementing TX interrupt
- **WHEN** reading interrupt section
- **THEN** guide SHALL explain byte-by-byte transmission
- **AND** guide SHALL show completion detection
- **AND** guide SHALL explain buffer ownership
- **AND** guide SHALL provide complete ISR example

### Requirement: Troubleshooting Guide
Documentation SHALL provide troubleshooting information.

#### Scenario: Common problems documented
- **GIVEN** developer encountering integration issue
- **WHEN** reading troubleshooting section
- **THEN** guide SHALL list common symptoms
- **AND** guide SHALL explain probable causes
- **AND** guide SHALL provide diagnostic steps
- **AND** guide SHALL show solutions

#### Scenario: Timing issues diagnosed
- **GIVEN** developer with timing problems
- **WHEN** reviewing timing troubleshooting
- **THEN** guide SHALL explain baud rate calculation errors
- **AND** guide SHALL show clock configuration issues
- **AND** guide SHALL explain frame timeout problems
- **AND** guide SHALL provide measurement techniques

### Requirement: Performance Optimization
Documentation SHALL explain performance optimization for MCUs.

#### Scenario: Memory optimization
- **GIVEN** developer with limited RAM
- **WHEN** reading optimization section
- **THEN** guide SHALL explain buffer sizing
- **AND** guide SHALL show static vs dynamic allocation
- **AND** guide SHALL explain FSM memory usage
- **AND** guide SHALL provide size estimates

#### Scenario: Speed optimization
- **GIVEN** developer optimizing performance
- **WHEN** reading optimization section
- **THEN** guide SHALL explain interrupt priority
- **AND** guide SHALL show DMA integration possibility
- **AND** guide SHALL explain cache considerations
- **AND** guide SHALL provide benchmarking tips

### Requirement: Build Integration
Examples SHALL demonstrate build system integration.

#### Scenario: CMake configuration
- **GIVEN** MCU example with CMake
- **WHEN** developer builds example
- **THEN** CMakeLists.txt SHALL be portable
- **AND** toolchain file SHALL be provided
- **AND** build SHALL succeed with documented tools
- **AND** configuration options SHALL be documented

#### Scenario: Makefile configuration
- **GIVEN** MCU example with Makefile
- **WHEN** developer builds example
- **THEN** Makefile SHALL be self-documented
- **AND** toolchain configuration SHALL be clear
- **AND** dependencies SHALL be minimal
- **AND** build outputs SHALL be explained

## MODIFIED Requirements

None - This is new documentation and examples.

## REMOVED Requirements

None.

## RENAMED Requirements

None.
