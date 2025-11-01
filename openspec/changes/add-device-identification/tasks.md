# Implementation Tasks

## 1. FC2B Protocol Module
- [ ] 1.1 Create `include/modbuscore/protocol/fc2b.h`
- [ ] 1.2 Define MEI type constants
- [ ] 1.3 Define device ID codes (Basic, Regular, Extended)
- [ ] 1.4 Define standard object IDs (0x00-0x06)
- [ ] 1.5 Implement request parsing
- [ ] 1.6 Implement response building
- [ ] 1.7 Handle multi-packet responses

## 2. Device Info Structure
- [ ] 2.1 Define device object structure
- [ ] 2.2 Define device info container
- [ ] 2.3 Implement object storage
- [ ] 2.4 Add conformity level tracking
- [ ] 2.5 Implement "more follows" logic

## 3. Device Info Management API
- [ ] 3.1 Add `mbc_server_set_device_info()` function
- [ ] 3.2 Add `mbc_server_add_device_object()` function
- [ ] 3.3 Implement object lookup by ID
- [ ] 3.4 Add conformity level configuration
- [ ] 3.5 Validate object IDs and data

## 4. Protocol Engine Integration
- [ ] 4.1 Add FC2B handler to request parser
- [ ] 4.2 Implement device info read logic
- [ ] 4.3 Build response with object data
- [ ] 4.4 Handle read device ID codes (0x01-0x04)
- [ ] 4.5 Test FC2B request/response flow

## 5. Standard Objects Support
- [ ] 5.1 Object 0x00: Vendor Name
- [ ] 5.2 Object 0x01: Product Code
- [ ] 5.3 Object 0x02: Major/Minor Revision
- [ ] 5.4 Object 0x03: Vendor URL (optional)
- [ ] 5.5 Object 0x04: Product Name (optional)
- [ ] 5.6 Object 0x05: Model Name (optional)
- [ ] 5.7 Object 0x06: User Application Name (optional)

## 6. Examples
- [ ] 6.1 Create basic device info example
- [ ] 6.2 Show regular conformity level setup
- [ ] 6.3 Demonstrate custom object addition
- [ ] 6.4 Add SCADA integration example
- [ ] 6.5 Write example READMEs

## 7. Documentation
- [ ] 7.1 Write FC2B integration guide
- [ ] 7.2 Document standard objects
- [ ] 7.3 Document conformity levels
- [ ] 7.4 Add troubleshooting section
- [ ] 7.5 Update API documentation

## 8. Testing
- [ ] 8.1 Unit tests for request parsing
- [ ] 8.2 Unit tests for response building
- [ ] 8.3 Integration tests for FC2B flow
- [ ] 8.4 Test multi-packet responses
- [ ] 8.5 Test all conformity levels
- [ ] 8.6 Performance benchmarking

## 9. Review
- [ ] 9.1 Code review
- [ ] 9.2 Documentation review
- [ ] 9.3 Update CHANGELOG
