# Complete MCU RTU Migration Plan - All OpenSpecs

## Executive Summary

Complete analysis and planning for migrating oldmodbus (Renesas RTU Slave) to modern ModbusCore library has been completed. **8 comprehensive OpenSpec specifications** have been created and validated, defining **303 total implementation tasks** across all improvements.

## Documents Created

### Analysis Documents
1. **MIGRATION_ANALYSIS.md** - Deep dive comparison of old vs new
2. **FEATURE_COMPARISON.md** - Feature-by-feature analysis
3. **MCU_MIGRATION_SUMMARY.md** - Initial 3-spec summary
4. **COMPLETE_MIGRATION_PLAN.md** - This document (final plan)

### OpenSpec Specifications (8 Total)

All specifications have been **validated with `openspec validate --strict`** ✅

## All OpenSpec Specifications

### Core Protocol Features (HIGH PRIORITY)

#### 1. **add-input-register-support** (33 tasks)
**Priority:** HIGH - Critical for industrial applications

**What:** FC04 (Read Input Registers) support
- Separate address space from holding registers
- Read callbacks for dynamic values
- Array registration
- Examples: temperature sensors, status flags

**Key Files:**
- `openspec/changes/add-input-register-support/`
- PDU module updates
- Server runtime API
- Sensor examples

**Estimated:** ~12 hours

---

#### 2. **add-device-identification** (48 tasks)
**Priority:** HIGH - Industry standard requirement

**What:** FC2B (Read Device Identification) support
- MEI type 0x0E implementation
- Standard objects (Vendor, Product, Version)
- Conformity levels (Basic, Regular, Extended)
- Multi-packet responses
- SCADA integration

**Key Files:**
- `openspec/changes/add-device-identification/`
- New FC2B protocol module
- Device info management
- SCADA examples

**Estimated:** ~18 hours

---

#### 3. **add-register-callbacks** (45 tasks)
**Priority:** HIGH - Essential for MCU flexibility

**What:** Read/Write callback system
- Read callbacks for dynamic values (ADC, sensors)
- Write callbacks for validation, persistence, hardware control
- Context parameter for state access
- Read-only protection
- Backward compatible (NULL callbacks = direct memory)

**Key Files:**
- `openspec/changes/add-register-callbacks/`
- Runtime API updates
- Callback examples (ADC, flash, motor control)

**Estimated:** ~15 hours

---

#### 4. **add-interrupt-driven-rx** (39 tasks)
**Priority:** HIGH - MCU efficiency requirement

**What:** ISR-safe byte/buffer injection
- `mbc_engine_inject_byte()` for UART RX ISR
- `mbc_engine_inject_buffer()` for DMA completion
- ISR safety guarantees (no malloc, bounded time)
- Frame timeout integration
- Reduced CPU usage vs polling

**Key Files:**
- `openspec/changes/add-interrupt-driven-rx/`
- Protocol engine updates
- ISR examples (RL78, STM32 DMA)
- Timer ISR patterns

**Estimated:** ~12 hours

---

#### 5. **add-custom-function-codes** (49 tasks)
**Priority:** HIGH - Vendor-specific requirements

**What:** Custom/vendor-specific function code support
- Register handlers for non-standard function codes
- FC41-FC43 support (broadcast commands)
- Request/response buffer access
- Exception handling
- Broadcast suppression

**Key Files:**
- `openspec/changes/add-custom-function-codes/`
- Handler registration API
- Dispatch logic
- Broadcast command examples

**Estimated:** ~18 hours

---

### MCU Integration (HIGH PRIORITY)

#### 6. **add-bootloader-integration** (30 tasks)
**Priority:** HIGH - Field update capability

**What:** Bootloader protocol integration
- Transport callback for custom frames
- FSM event for bootloader handling
- Two-stage bootloader protocol
- CRC and boot swap logic
- Renesas RL78 example

**Key Files:**
- `openspec/changes/add-bootloader-integration/`
- Transport callback
- FSM bootloader event
- RL78 bootloader example

**Estimated:** ~12 hours

---

#### 7. **add-mcu-integration-guide** (38 tasks)
**Priority:** HIGH - Developer productivity

**What:** Comprehensive MCU integration documentation
- Step-by-step integration guide
- Renesas RL78 complete example
- Additional MCU example (STM32/AVR)
- RS485 control variations (DE/RE pin vs pure TX/RX)
- Template code
- Troubleshooting guide

**Key Files:**
- `openspec/changes/add-mcu-integration-guide/`
- docs/getting-started/mcu-integration.md
- examples/renesas_rl78/
- examples/stm32f1/ or examples/avr/
- examples/templates/

**Estimated:** ~20 hours

---

### Application Patterns (MEDIUM PRIORITY)

#### 8. **add-comm-watchdog-pattern** (21 tasks)
**Priority:** MEDIUM - Safety feature

**What:** Communication timeout monitoring pattern
- Broken cable mode documentation
- Timer management strategies
- Multi-register watchdog
- Integration with write callbacks
- Emergency speed fallback example

**Key Files:**
- `openspec/changes/add-comm-watchdog-pattern/`
- docs/advanced/comm-watchdog-pattern.md
- examples/broken_cable_mode/
- Multi-register example

**Estimated:** ~8 hours

---

## Summary Statistics

### Total Workload
- **Specifications:** 8
- **Total Tasks:** 303
- **Estimated Hours:** ~115 hours
- **Estimated Duration:** 3-4 weeks (1 developer)

### Task Breakdown by Spec
```
add-custom-function-codes       49 tasks  (16%)
add-device-identification       48 tasks  (16%)
add-register-callbacks          45 tasks  (15%)
add-interrupt-driven-rx         39 tasks  (13%)
add-mcu-integration-guide       38 tasks  (13%)
add-input-register-support      33 tasks  (11%)
add-bootloader-integration      30 tasks  (10%)
add-comm-watchdog-pattern       21 tasks  ( 7%)
```

### Priority Distribution
- **HIGH Priority:** 7 specs (87%)
- **MEDIUM Priority:** 1 spec (13%)

## Comparison: Before vs After

### Function Code Support

| Function Code | Old Lib | Current | After Migration |
|---------------|---------|---------|-----------------|
| FC01: Read Coils | ✅ | ❌ | ⏳ Future |
| FC02: Read Discrete Inputs | ✅ | ❌ | ⏳ Future |
| FC03: Read Holding Registers | ✅ | ✅ | ✅ |
| **FC04: Read Input Registers** | ✅ | ❌ | ✅ **NEW** |
| FC05: Write Single Coil | ✅ | ❌ | ⏳ Future |
| FC06: Write Single Register | ✅ | ✅ | ✅ |
| FC0F: Write Multiple Coils | ✅ | ❌ | ⏳ Future |
| FC10: Write Multiple Registers | ✅ | ✅ | ✅ |
| FC17: Read/Write Multiple | ✅ | ❌ | ⏳ Future |
| **FC2B: Device Identification** | ✅ | ❌ | ✅ **NEW** |
| **FC41-FC43: Custom Broadcast** | ✅ | ❌ | ✅ **NEW** |

### Feature Comparison

| Feature | Old Lib | Current | After Migration |
|---------|---------|---------|-----------------|
| Holding Registers | ✅ | ✅ | ✅ |
| **Input Registers** | ✅ | ❌ | ✅ **NEW** |
| **Register Callbacks** | ✅ | ❌ | ✅ **NEW** |
| **Device Identification** | ✅ | ❌ | ✅ **NEW** |
| **Interrupt RX** | ✅ | ❌ | ✅ **NEW** |
| **Custom Function Codes** | ✅ | ❌ | ✅ **NEW** |
| **Bootloader Integration** | ✅ | ❌ | ✅ **NEW** |
| **MCU Examples** | ✅ (RL78) | ❌ | ✅ (RL78, STM32/AVR) |
| Communication Watchdog | ✅ | ❌ | ✅ (Documented pattern) |
| RS485 DE/RE Control | ✅ | ⏳ Partial | ✅ (Documented) |
| Platform Agnostic | ❌ | ✅ | ✅ |
| Modern Architecture | ❌ | ✅ | ✅ |
| Comprehensive Tests | ❌ | ✅ | ✅ |

## Implementation Phases

### Phase 1: Core Protocol (Weeks 1-2)
**Priority:** Critical features for protocol completeness

**Specs:**
1. add-input-register-support (33 tasks)
2. add-device-identification (48 tasks)
3. add-register-callbacks (45 tasks)

**Deliverables:**
- FC04 working
- FC2B working
- Callback system functional
- All tests passing

**Total:** 126 tasks, ~45 hours

---

### Phase 2: MCU Efficiency (Weeks 2-3)
**Priority:** Essential for MCU performance

**Specs:**
1. add-interrupt-driven-rx (39 tasks)
2. add-custom-function-codes (49 tasks)
3. add-bootloader-integration (30 tasks)

**Deliverables:**
- ISR injection working
- Custom FC dispatch working
- Bootloader callback functional
- ISR examples complete

**Total:** 118 tasks, ~42 hours

---

### Phase 3: Integration & Documentation (Weeks 3-4)
**Priority:** Developer productivity

**Specs:**
1. add-mcu-integration-guide (38 tasks)
2. add-comm-watchdog-pattern (21 tasks)

**Deliverables:**
- Complete MCU integration guide
- 2+ MCU examples (RL78, STM32/AVR)
- Watchdog pattern documented
- All examples tested on real hardware

**Total:** 59 tasks, ~28 hours

---

## Success Criteria

The migration is successful when:

1. ✅ All 8 OpenSpecs implemented and tested
2. ✅ All 303 tasks completed
3. ✅ FC04 (Input Registers) working
4. ✅ FC2B (Device ID) working
5. ✅ Register callbacks functional
6. ✅ Interrupt-driven RX working
7. ✅ Custom function codes supported
8. ✅ Bootloader integration working
9. ✅ At least 2 MCU examples exist (RL78 + STM32/AVR)
10. ✅ All tests passing
11. ✅ Documentation complete
12. ✅ Performance >= oldmodbus
13. ✅ Can replace oldmodbus in production

## Risk Assessment

### Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Register callback overhead | LOW | MEDIUM | Benchmark early, NULL check optimization |
| ISR timing issues | MEDIUM | HIGH | Test on real hardware, document constraints |
| Custom FC security | MEDIUM | HIGH | Buffer bounds checking, extensive validation |
| FC2B complexity | MEDIUM | MEDIUM | Follow spec strictly, test with real SCADA |
| Integration bugs | MEDIUM | MEDIUM | Multiple MCU examples, thorough testing |

### Schedule Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Underestimated complexity | MEDIUM | MEDIUM | 20% buffer built into estimates |
| Hardware availability | LOW | LOW | Start with available RL78 |
| Scope creep | MEDIUM | HIGH | Stick to specs, defer extras to future |
| Testing time | HIGH | MEDIUM | Parallel testing, automated where possible |

## Next Steps

### Immediate Actions (Today)

1. **Review all 8 OpenSpecs**
   ```bash
   cd openspec/changes
   ls -d add-*/

   # Review each:
   cat add-input-register-support/proposal.md
   cat add-device-identification/proposal.md
   cat add-register-callbacks/proposal.md
   cat add-interrupt-driven-rx/proposal.md
   cat add-custom-function-codes/proposal.md
   cat add-bootloader-integration/proposal.md
   cat add-mcu-integration-guide/proposal.md
   cat add-comm-watchdog-pattern/proposal.md
   ```

2. **Validate all specs**
   ```bash
   openspec validate --strict
   # All should pass ✅
   ```

3. **Approve specifications**
   - Review proposals with team
   - Approve to proceed with implementation

### Week 1: Start Phase 1

1. **Implement FC04 (Input Registers)**
   - Follow tasks in add-input-register-support/tasks.md
   - Test thoroughly
   - Mark tasks as completed

2. **Begin FC2B (Device ID)**
   - Start protocol module
   - Implement basic objects

### Ongoing

- Update task status in tasks.md files
- Run tests continuously
- Document as you go
- Commit incrementally

## File Organization

```
modbus-library/
├── docs/
│   ├── MIGRATION_ANALYSIS.md          ✅ Created
│   ├── FEATURE_COMPARISON.md          ✅ Created
│   ├── MCU_MIGRATION_SUMMARY.md       ✅ Created
│   ├── COMPLETE_MIGRATION_PLAN.md     ✅ Created (this file)
│   ├── advanced/
│   │   ├── bootloader-integration.md  ⏳ Spec created, pending implementation
│   │   ├── comm-watchdog-pattern.md   ⏳ Spec created, pending implementation
│   │   └── interrupt-driven-rx.md     ⏳ Spec created, pending implementation
│   └── getting-started/
│       └── mcu-integration.md         ⏳ Spec created, pending implementation
│
├── include/modbuscore/
│   ├── protocol/
│   │   ├── pdu.h                      ⏳ FC04 functions to add
│   │   └── fc2b.h                     ⏳ New file
│   └── runtime/
│       └── runtime.h                  ⏳ APIs to add
│
├── src/
│   ├── protocol/
│   │   ├── pdu.c                      ⏳ FC04 implementation
│   │   ├── fc2b.c                     ⏳ New file
│   │   └── engine.c                   ⏳ Updates for custom FC, ISR
│   └── runtime/
│       ├── server.c                   ⏳ Register management updates
│       ├── device_info.c              ⏳ New file
│       └── custom_fc.c                ⏳ New file
│
├── examples/
│   ├── renesas_rl78/                  ⏳ To create
│   ├── renesas_rl78_bootloader/       ⏳ To create
│   ├── stm32f1/ (or avr_atmega/)      ⏳ To create
│   ├── broken_cable_mode/             ⏳ To create
│   ├── callbacks/                     ⏳ To create
│   ├── custom_fc/                     ⏳ To create
│   └── templates/                     ⏳ To create
│
├── openspec/
│   └── changes/
│       ├── add-input-register-support/       ✅ Created & validated
│       ├── add-device-identification/        ✅ Created & validated
│       ├── add-register-callbacks/           ✅ Created & validated
│       ├── add-interrupt-driven-rx/          ✅ Created & validated
│       ├── add-custom-function-codes/        ✅ Created & validated
│       ├── add-bootloader-integration/       ✅ Created & validated
│       ├── add-mcu-integration-guide/        ✅ Created & validated
│       └── add-comm-watchdog-pattern/        ✅ Created & validated
│
└── oldmodbus/                         ✅ Reference implementation
```

## Commands Reference

```bash
# List all changes
openspec list

# View specific change
openspec show add-input-register-support
openspec show add-device-identification
openspec show add-register-callbacks
openspec show add-interrupt-driven-rx
openspec show add-custom-function-codes

# Validate all
openspec validate --strict

# After implementation
openspec archive add-input-register-support --yes
# ... repeat for each spec
```

## Conclusion

A comprehensive plan is now in place to fully modernize the Modbus RTU implementation for MCUs. The **8 OpenSpec specifications** provide:

- ✅ **Clear requirements** - Every feature well-defined
- ✅ **Incremental tasks** - 303 actionable steps
- ✅ **Acceptance criteria** - Scenarios for every requirement
- ✅ **Complete coverage** - All oldmodbus features accounted for
- ✅ **Quality gates** - Validation at each step

The library will achieve:
- **Feature Parity** - All critical oldmodbus features
- **Enhanced Flexibility** - Custom FCs, callbacks, interrupts
- **Better Architecture** - Platform-agnostic, modular design
- **Superior Documentation** - Guides, examples, troubleshooting
- **Production Ready** - Tested, validated, industrial-grade

**Status:** ✅ Planning Complete - Ready for Implementation

---

**Document Version:** 1.0
**Date:** 2025-01-28
**Total Specs:** 8 (all validated)
**Total Tasks:** 303
**Estimated Effort:** ~115 hours (~3-4 weeks)
**Next Review:** After Phase 1 completion
