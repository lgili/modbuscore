# Implementation Tasks

## 1. Transport Layer Updates
- [ ] 1.1 Add `parse_bootloader_request` callback to `modbus_transport_t` structure
- [ ] 1.2 Define callback signature and return codes (SUCCESS, ERROR, HANDLED)
- [ ] 1.3 Update transport layer documentation
- [ ] 1.4 Add NULL safety checks for callback

## 2. FSM Event Handling
- [ ] 2.1 Add `MODBUS_EVENT_BOOTLOADER` to FSM event enum
- [ ] 2.2 Add state transition from PROCESSING to SENDING for bootloader events
- [ ] 2.3 Implement early-exit path in frame processing
- [ ] 2.4 Add tests for bootloader event flow

## 3. Frame Interception Logic
- [ ] 3.1 Add callback invocation in `action_process_frame()`
- [ ] 3.2 Implement return code handling (continue normal path vs bypass)
- [ ] 3.3 Add buffer management for custom responses
- [ ] 3.4 Verify CRC handling for custom frames

## 4. Documentation
- [ ] 4.1 Write bootloader integration guide in `docs/advanced/bootloader-integration.md`
- [ ] 4.2 Document protocol patterns and best practices
- [ ] 4.3 Add sequence diagrams for bootloader flow
- [ ] 4.4 Update API reference

## 5. Example Implementation
- [ ] 5.1 Create `examples/renesas_rl78_bootloader/` directory
- [ ] 5.2 Port old bootloader protocol logic
- [ ] 5.3 Implement two-stage bootloader activation
- [ ] 5.4 Add CRC calculation and boot swap logic
- [ ] 5.5 Write example README with wiring diagrams

## 6. Testing
- [ ] 6.1 Unit tests for callback invocation
- [ ] 6.2 Integration tests for bootloader flow
- [ ] 6.3 Test NULL callback (normal operation)
- [ ] 6.4 Test interrupt safety
- [ ] 6.5 Verify no memory leaks

## 7. Review and Documentation
- [ ] 7.1 Code review
- [ ] 7.2 Performance benchmarking (ensure <1% overhead)
- [ ] 7.3 Update CHANGELOG
- [ ] 7.4 Update migration guide
