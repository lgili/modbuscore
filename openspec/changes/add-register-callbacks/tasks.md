# Implementation Tasks

## 1. Callback Signatures
- [ ] 1.1 Define `mbc_read_callback_t` signature
- [ ] 1.2 Define `mbc_write_callback_t` signature
- [ ] 1.3 Add context parameter to callbacks
- [ ] 1.4 Document callback contracts
- [ ] 1.5 Add callback return codes

## 2. Register Structure Updates
- [ ] 2.1 Add read_callback field to holding register
- [ ] 2.2 Add write_callback field to holding register
- [ ] 2.3 Add read_callback field to input register
- [ ] 2.4 Add callback_context field
- [ ] 2.5 Add read_only flag

## 3. API Updates
- [ ] 3.1 Update `mbc_server_set_holding_register()` with callbacks
- [ ] 3.2 Update `mbc_server_set_holding_register_array()` with callbacks
- [ ] 3.3 Update `mbc_server_set_input_register()` with callbacks
- [ ] 3.4 Update `mbc_server_set_input_register_array()` with callbacks
- [ ] 3.5 Add convenience functions for direct memory mapping

## 4. Callback Invocation Logic
- [ ] 4.1 Implement read callback invocation for holding registers
- [ ] 4.2 Implement read callback invocation for input registers
- [ ] 4.3 Implement write callback invocation for holding registers
- [ ] 4.4 Handle NULL callbacks (direct memory access)
- [ ] 4.5 Pass context parameter to callbacks
- [ ] 4.6 Handle callback return codes

## 5. Validation and Protection
- [ ] 5.1 Enforce read-only flag on writes
- [ ] 5.2 Allow write callbacks to reject values
- [ ] 5.3 Return exception on validation failure
- [ ] 5.4 Add debug logging for callback errors

## 6. Examples
- [ ] 6.1 Create ADC read callback example
- [ ] 6.2 Create write validation example
- [ ] 6.3 Create flash persistence example
- [ ] 6.4 Create motor control example
- [ ] 6.5 Create read-only protection example
- [ ] 6.6 Add example READMEs

## 7. Documentation
- [ ] 7.1 Write callback usage guide
- [ ] 7.2 Document callback patterns
- [ ] 7.3 Document thread safety considerations
- [ ] 7.4 Add troubleshooting section
- [ ] 7.5 Update API reference

## 8. Testing
- [ ] 8.1 Unit tests for read callbacks
- [ ] 8.2 Unit tests for write callbacks
- [ ] 8.3 Unit tests for NULL callback fallback
- [ ] 8.4 Unit tests for validation rejection
- [ ] 8.5 Integration tests for callback flow
- [ ] 8.6 Performance benchmarking

## 9. Review
- [ ] 9.1 Code review
- [ ] 9.2 Documentation review
- [ ] 9.3 Update CHANGELOG
