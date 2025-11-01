# Implementation Tasks

## 1. PDU Module Updates
- [ ] 1.1 Add `mbc_pdu_build_read_input_request()` function
- [ ] 1.2 Add `mbc_pdu_parse_read_input_response()` function
- [ ] 1.3 Add FC04 constant definition
- [ ] 1.4 Update PDU tests for FC04

## 2. Runtime/Server API
- [ ] 2.1 Add input register structure to runtime context
- [ ] 2.2 Implement `mbc_server_set_input_register()` function
- [ ] 2.3 Implement `mbc_server_set_input_register_array()` function
- [ ] 2.4 Add input register lookup function
- [ ] 2.5 Add input register count tracking

## 3. Protocol Engine Integration
- [ ] 3.1 Add FC04 handling to request parser
- [ ] 3.2 Implement input register read logic
- [ ] 3.3 Add response building for FC04
- [ ] 3.4 Test FC04 request/response flow

## 4. Callback Support
- [ ] 4.1 Define input register callback signature
- [ ] 4.2 Implement callback invocation on read
- [ ] 4.3 Support NULL callbacks (direct memory read)
- [ ] 4.4 Test callback execution

## 5. Examples
- [ ] 5.1 Create temperature sensor example
- [ ] 5.2 Add multi-sensor example
- [ ] 5.3 Demonstrate callback usage
- [ ] 5.4 Add example README

## 6. Documentation
- [ ] 6.1 Update API documentation
- [ ] 6.2 Add input register guide
- [ ] 6.3 Document address space separation
- [ ] 6.4 Add troubleshooting section

## 7. Testing
- [ ] 7.1 Unit tests for PDU functions
- [ ] 7.2 Integration tests for server
- [ ] 7.3 Test address space isolation
- [ ] 7.4 Test callback execution
- [ ] 7.5 Performance benchmarking

## 8. Review
- [ ] 8.1 Code review
- [ ] 8.2 Documentation review
- [ ] 8.3 Update CHANGELOG
