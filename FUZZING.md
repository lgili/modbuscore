# Fuzzing Infrastructure

ModbusCore includes comprehensive fuzzing infrastructure to ensure parser robustness and security.

## Quick Start

**On macOS (Docker - Recommended):**

```bash
# Quick test (60 seconds)
./scripts/fuzz_in_docker.sh

# Longer campaign (5 minutes)
./scripts/fuzz_in_docker.sh 300
```

**On Linux:**

```bash
# 1. Build with fuzzing enabled
cmake -B build -DCMAKE_C_COMPILER=clang -DENABLE_FUZZING=ON
cmake --build build

# 2. Generate seed corpus
cd build/tests/fuzz
make generate_fuzz_corpus

# 3. Run fuzzer
./fuzz_mbap_decoder -max_total_time=300 corpus/
```

## What Gets Fuzzed?

| Component | Fuzzer | Lines of Code | Risk Level |
|-----------|--------|---------------|------------|
| MBAP decoder | `fuzz_mbap_decoder` | ~150 LOC | High (network-facing) |
| RTU CRC | `fuzz_rtu_crc` | ~80 LOC | Medium (checksums critical) |
| PDU parser | `fuzz_pdu_parser` | ~400 LOC | High (complex state machine) |

## CI/CD Integration

Fuzzing runs automatically:
- ✅ **On every PR**: 60-second smoke test
- ✅ **Weekly**: 6-hour deep fuzzing
- ✅ **Artifacts**: Crash files saved for 90 days

See `.github/workflows/fuzz.yml` for configuration.

## Phase 7 Goals

- [x] Implement LibFuzzer harnesses for all parsers
- [x] Integrate fuzzing into CI/CD pipeline
- [x] Generate seed corpus with valid Modbus frames
- [ ] **Achieve 1 billion executions without crashes** (in progress)
- [ ] Document and fix any discovered vulnerabilities

## Documentation

Detailed fuzzing guide: [`tests/fuzz/README.md`](tests/fuzz/README.md)

## Results

Current fuzzing statistics (updated weekly):

| Fuzzer | Total Execs | Crashes | Coverage |
|--------|-------------|---------|----------|
| fuzz_mbap_decoder | TBD | 0 | TBD% |
| fuzz_rtu_crc | TBD | 0 | TBD% |
| fuzz_pdu_parser | TBD | 0 | TBD% |

_(Run `make update_fuzz_stats` to update this table)_

## Contributing

Found a crash? Please report:
1. Attach the `crash-*` file
2. Describe reproduction steps
3. Indicate severity (DoS, RCE, memory corruption, etc.)

Security vulnerabilities: Contact maintainers privately before public disclosure.
