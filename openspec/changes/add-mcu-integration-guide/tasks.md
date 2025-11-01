# Implementation Tasks

## 1. Integration Guide Documentation
- [ ] 1.1 Create `docs/getting-started/mcu-integration.md`
- [ ] 1.2 Document transport layer requirements
- [ ] 1.3 Explain UART configuration steps
- [ ] 1.4 Show interrupt vs polling trade-offs
- [ ] 1.5 Document timing requirements
- [ ] 1.6 Add RS485 control patterns
- [ ] 1.7 Create troubleshooting section

## 2. Renesas RL78 Example
- [ ] 2.1 Create `examples/renesas_rl78/` directory
- [ ] 2.2 Port UART configuration from oldmodbus
- [ ] 2.3 Implement transport layer callbacks
- [ ] 2.4 Add RS485 DE/RE control
- [ ] 2.5 Show interrupt-driven RX/TX
- [ ] 2.6 Add CMake or makefile
- [ ] 2.7 Write detailed README with wiring

## 3. Second MCU Example (STM32 or AVR)
- [ ] 3.1 Choose target MCU family
- [ ] 3.2 Create example directory
- [ ] 3.3 Implement transport layer
- [ ] 3.4 Show different RS485 pattern (if applicable)
- [ ] 3.5 Add build configuration
- [ ] 3.6 Write README

## 4. Template Code
- [ ] 4.1 Create `examples/templates/` directory
- [ ] 4.2 Add transport layer template
- [ ] 4.3 Add interrupt handler template
- [ ] 4.4 Add configuration checklist
- [ ] 4.5 Add common pitfalls document

## 5. Integration Patterns
- [ ] 5.1 Document signal inversion handling
- [ ] 5.2 Show baud rate configuration patterns
- [ ] 5.3 Explain watchdog recovery
- [ ] 5.4 Document buffer sizing
- [ ] 5.5 Show performance optimization tips

## 6. Testing
- [ ] 6.1 Verify examples compile on real toolchains
- [ ] 6.2 Test on physical hardware
- [ ] 6.3 Verify Modbus compliance
- [ ] 6.4 Test interrupt timing

## 7. Review
- [ ] 7.1 Code review
- [ ] 7.2 Documentation review
- [ ] 7.3 Community feedback
- [ ] 7.4 Update main README
