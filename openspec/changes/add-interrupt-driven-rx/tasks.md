# Implementation Tasks

## 1. Protocol Engine API
- [ ] 1.1 Add `mbc_engine_inject_byte()` function
- [ ] 1.2 Add `mbc_engine_inject_buffer()` function
- [ ] 1.3 Ensure ISR safety (no malloc, no blocking)
- [ ] 1.4 Add return codes for buffer full, etc.
- [ ] 1.5 Document ISR requirements

## 2. Byte Injection Logic
- [ ] 2.1 Implement single byte injection
- [ ] 2.2 Implement buffer injection
- [ ] 2.3 Update RX buffer management
- [ ] 2.4 Handle buffer overflow gracefully
- [ ] 2.5 Trigger frame processing on complete frame

## 3. Frame Timeout Handling
- [ ] 3.1 Document timeout behavior with interrupts
- [ ] 3.2 Show how to use timer ISR for timeout
- [ ] 3.3 Add timeout injection API (optional)
- [ ] 3.4 Test timeout edge cases

## 4. ISR Safety Verification
- [ ] 4.1 Audit for malloc/free
- [ ] 4.2 Audit for blocking calls
- [ ] 4.3 Verify atomic operations where needed
- [ ] 4.4 Document reentrant requirements
- [ ] 4.5 Add static analysis checks

## 5. Examples
- [ ] 5.1 Create UART RX ISR example (Renesas RL78)
- [ ] 5.2 Create DMA completion example (STM32)
- [ ] 5.3 Show timer ISR for frame timeout
- [ ] 5.4 Add circular buffer example
- [ ] 5.5 Write example READMEs

## 6. Documentation
- [ ] 6.1 Write interrupt-driven guide
- [ ] 6.2 Document ISR timing requirements
- [ ] 6.3 Document ISR safety guarantees
- [ ] 6.4 Add troubleshooting section
- [ ] 6.5 Update API reference

## 7. Testing
- [ ] 7.1 Unit tests for byte injection
- [ ] 7.2 Unit tests for buffer injection
- [ ] 7.3 Test buffer overflow handling
- [ ] 7.4 Test frame timeout
- [ ] 7.5 Integration tests with simulated ISR
- [ ] 7.6 Performance benchmarking vs polling

## 8. Review
- [ ] 8.1 Code review
- [ ] 8.2 ISR safety review
- [ ] 8.3 Documentation review
- [ ] 8.4 Update CHANGELOG
