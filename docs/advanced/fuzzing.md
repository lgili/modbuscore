# Fuzzing & Robustness Testing

Complete guide to fuzzing ModbusCore for security and reliability testing using LibFuzzer and AFL++.

## Table of Contents

- [Overview](#overview)
- [Why Fuzz ModbusCore?](#why-fuzz-modbuscore)
- [Fuzzing Infrastructure](#fuzzing-infrastructure)
- [Quick Start](#quick-start)
- [Building Fuzzers](#building-fuzzers)
- [Running Fuzzing Campaigns](#running-fuzzing-campaigns)
- [Coverage Analysis](#coverage-analysis)
- [Interpreting Results](#interpreting-results)
- [CI/CD Integration](#cicd-integration)
- [Alternative Fuzzers](#alternative-fuzzers)
- [Best Practices](#best-practices)
- [Troubleshooting](#troubleshooting)

---

## Overview

**Fuzzing** (or fuzz testing) is an automated software testing technique that feeds invalid, unexpected, or random data to a program to discover bugs, crashes, memory leaks, and security vulnerabilities.

### What ModbusCore Fuzzing Tests

| Component | Coverage | Potential Issues |
|-----------|----------|------------------|
| **MBAP Decoder** | TCP frame parsing, header validation | Buffer overflows, integer overflows, malformed frames |
| **RTU CRC** | CRC16 calculation, validation logic | Integer errors, edge cases, performance issues |
| **PDU Parser** | All function codes (read/write registers) | Out-of-bounds access, invalid FC handling |
| **Transport Layer** | Connection handling, socket I/O | Race conditions, resource leaks |

### Security Benefits

- **Early Bug Detection**: Find crashes before production deployment
- **Attack Surface Mapping**: Identify input paths vulnerable to exploitation
- **Regression Prevention**: Continuous fuzzing catches regressions from code changes
- **Compliance**: Demonstrate security testing for ICS/SCADA certifications

---

## Why Fuzz ModbusCore?

### Real-World Attack Vectors

Modbus implementations are common targets in industrial environments:

```
Attack Scenario: Malicious Modbus Frame
┌────────────────────────────────────────┐
│ Attacker crafts invalid MBAP frame:   │
│   Transaction ID: 0xFFFF              │
│   Protocol ID: 0x1234 (invalid!)      │
│   Length: 0x0001 (underflow)          │
│   Unit ID: 0x00                       │
│   PDU: <empty>                        │
└────────────────────────────────────────┘
         │
         ├──> Without fuzzing: Potential crash, DoS
         └──> With fuzzing: Detected & fixed before deployment
```

### Historical CVEs in Modbus Stacks

- **CVE-2019-6535**: Buffer overflow in libmodbus (TCP)
- **CVE-2020-15368**: DoS via malformed RTU frames
- **CVE-2021-32990**: Integer overflow in function code parser

Fuzzing helps prevent these classes of vulnerabilities.

---

## Fuzzing Infrastructure

ModbusCore includes production-ready fuzzing harnesses in `tests/fuzz/`:

### Available Fuzzers

#### 1. `fuzz_mbap_decoder` - TCP Frame Parser

Tests MBAP (Modbus Application Protocol) header decoding:

```c
// Harness structure (simplified)
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    mbc_mbap_header_t header = {0};
    const uint8_t* pdu = NULL;
    size_t pdu_length = 0;

    // Test decoding arbitrary input
    mbc_status_t status = mbc_mbap_decode(data, size, &header, &pdu, &pdu_length);

    // Validate consistency: if OK, PDU pointer must be valid
    if (status == MBC_STATUS_OK && pdu != NULL) {
        volatile uint8_t check = pdu[0]; // Trigger ASAN on bad pointer
    }

    return 0; // Always accept input for corpus
}
```

**What It Tests:**
- Malformed MBAP headers (invalid protocol ID, length fields)
- Truncated frames (partial headers)
- Oversized frames (>260 bytes)
- Null pointer handling
- Boundary conditions (exact header size, max frame size)

#### 2. `fuzz_rtu_crc` - CRC16 Calculation

Tests RTU CRC computation and validation:

```c
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 2) return 0;

    // Test CRC calculation on arbitrary data
    uint16_t crc = mbc_crc16(data, size - 2);

    // Test CRC validation (compare with last 2 bytes)
    bool valid = mbc_crc16_validate(data, size);

    // Edge case: single-byte frames
    if (size == 3) {
        mbc_crc16(data, 1);
    }

    return 0;
}
```

**What It Tests:**
- CRC calculation on various data lengths (0 to 4096 bytes)
- CRC validation logic (matching/mismatching checksums)
- Edge cases (single-byte frames, maximum-length frames)
- Performance (no algorithmic complexity bugs)

#### 3. `fuzz_pdu_parser` - Function Code Handlers

Tests PDU encoding/decoding for all function codes:

```c
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1) return 0;

    uint8_t fc = data[0];
    const uint8_t* payload = data + 1;
    size_t payload_size = size - 1;

    // Test PDU decoding
    mbc_pdu_t pdu = {0};
    mbc_pdu_decode(data, size, &pdu);

    // Test function code parsers (FC03, FC06, FC16, etc.)
    switch (fc) {
        case 0x03: // Read Holding Registers
            if (payload_size >= 4) {
                uint16_t addr = (payload[0] << 8) | payload[1];
                uint16_t count = (payload[2] << 8) | payload[3];
                // Validate ranges
            }
            break;
        // ... other function codes
    }

    return 0;
}
```

**What It Tests:**
- All 10 supported function codes (FC01-FC06, FC15-FC17, FC23)
- Invalid function codes (0x00, 0x80+, unimplemented)
- Malformed PDU payloads (wrong lengths, invalid addresses)
- Exception response parsing

---

## Quick Start

### Prerequisites

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install -y clang cmake python3

# Or macOS (note: ARM64 has LibFuzzer issues, use Docker)
brew install llvm cmake python3
```

### 60-Second Smoke Test

```bash
# Configure with fuzzing enabled
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DENABLE_FUZZING=ON

# Build all fuzzers
cmake --build build --parallel

# Generate seed corpus (valid Modbus frames)
cd build/tests/fuzz
./generate_corpus
mkdir -p corpus && mv *.bin corpus/

# Run fuzzer for 60 seconds
./fuzz_mbap_decoder -max_total_time=60 corpus/
```

**Expected Output:**
```
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 3745387534
INFO: Loaded 1 modules   (215 inline 8-bit counters): 215 [0x5653e8b03020, 0x5653e8b030f7),
INFO: Loaded 1 PC tables (215 PCs): 215 [0x5653e8b030f8,0x5653e8b03868),
INFO:       15 files found in corpus
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: seed corpus: files: 15 min: 7b max: 260b total: 1456b rss: 31Mb
#16     INITED cov: 89 ft: 94 corp: 15/1456b exec/s: 0 rss: 32Mb
#10000  pulse  cov: 89 ft: 94 corp: 15/1456b lim: 260 exec/s: 5000 rss: 33Mb
#20000  pulse  cov: 91 ft: 98 corp: 17/1523b lim: 260 exec/s: 6666 rss: 33Mb
#40000  pulse  cov: 93 ft: 103 corp: 19/1687b lim: 260 exec/s: 8000 rss: 34Mb
Done 60000 runs in 60 seconds
#60000  DONE   cov: 95 ft: 107 corp: 21/1789b lim: 260 exec/s: 1000 rss: 35Mb
```

✅ **No crashes = robust code!**

---

## Building Fuzzers

### Linux/macOS Build

```bash
# Configure
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DENABLE_FUZZING=ON \
  -DBUILD_TESTING=OFF

# Build specific fuzzer
cmake --build build --target fuzz_mbap_decoder

# Build all fuzzers
cmake --build build --target all
```

### Docker Build (Recommended for macOS ARM64)

```bash
# Quick test (60 seconds)
./scripts/fuzz_in_docker.sh

# Custom duration (5 minutes = 300 seconds)
./scripts/fuzz_in_docker.sh 300

# Specific fuzzer only
./scripts/fuzz_in_docker.sh 60 fuzz_rtu_crc

# Long campaign (1 hour)
./scripts/fuzz_in_docker.sh 3600
```

**What the Docker Script Does:**
1. Creates Ubuntu container with Clang/LibFuzzer
2. Builds all fuzzing targets
3. Generates seed corpus
4. Runs fuzzing campaign
5. Saves results to `fuzz_results_<timestamp>/`
6. Extracts any crashes for analysis

### Custom Sanitizers

#### AddressSanitizer (ASAN) - Default

Detects:
- Buffer overflows (heap/stack)
- Use-after-free
- Double-free
- Memory leaks

```bash
# Already enabled in -DENABLE_FUZZING=ON
cmake -B build -DCMAKE_C_COMPILER=clang -DENABLE_FUZZING=ON
```

#### UndefinedBehaviorSanitizer (UBSAN)

Detects:
- Integer overflows
- Null pointer dereference
- Misaligned memory access
- Signed integer overflow

```bash
# Add UBSAN flags
cmake -B build \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_C_FLAGS="-fsanitize=fuzzer,address,undefined" \
  -DENABLE_FUZZING=ON
```

#### MemorySanitizer (MSAN)

Detects uninitialized memory reads (requires recompiling all dependencies):

```bash
cmake -B build \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_C_FLAGS="-fsanitize=fuzzer,memory -fsanitize-memory-track-origins" \
  -DENABLE_FUZZING=ON
```

---

## Running Fuzzing Campaigns

### Quick Test (CI-Friendly)

```bash
cd build/tests/fuzz

# Run for 60 seconds
./fuzz_mbap_decoder -max_total_time=60 corpus/
```

### Extended Run (Target: 1 Billion Executions)

```bash
# Run until 1 billion executions (may take hours/days)
./fuzz_mbap_decoder \
  -runs=1000000000 \
  -print_final_stats=1 \
  -print_corpus_stats=1 \
  -timeout=10 \
  -rss_limit_mb=2048 \
  corpus/
```

**Progress Tracking:**
```bash
# Check cumulative execution count
grep "DONE" fuzz_*.log | awk '{sum += $2} END {print sum " total executions"}'
```

### Parallel Fuzzing (Multi-Core)

```bash
# Use all CPU cores
./fuzz_mbap_decoder -jobs=$(nproc) -workers=$(nproc) corpus/

# Custom job count (e.g., 8 cores)
./fuzz_mbap_decoder -jobs=8 -workers=8 corpus/
```

**Performance Comparison:**
| Mode | Executions/Sec (4-core) | Time to 1B |
|------|-------------------------|------------|
| Single-core | ~2,000 | 138 hours |
| 4-core parallel | ~7,500 | 37 hours |

### Continuous Fuzzing

Run fuzzing in the background indefinitely:

```bash
# Start detached screen session
screen -S fuzzing

# Run without time limit
./fuzz_mbap_decoder corpus/ 2>&1 | tee fuzz_mbap.log

# Detach: Ctrl+A, then D
# Reattach: screen -r fuzzing
```

### Advanced Options

#### Custom Input Length

```bash
# Test only small frames (7-64 bytes)
./fuzz_mbap_decoder -max_len=64 corpus/

# Test large frames (up to 4KB)
./fuzz_pdu_parser -max_len=4096 corpus/
```

#### Detect Slow Inputs

```bash
# Report inputs that take >100ms to process
./fuzz_mbap_decoder -report_slow_units=100 corpus/
```

#### Verbosity & Debugging

```bash
# Detailed logging
./fuzz_mbap_decoder -verbosity=2 -print_pcs=1 corpus/

# Print coverage information
./fuzz_mbap_decoder -print_coverage=1 corpus/
```

#### Custom Dictionaries

Create `modbus.dict` with Modbus-specific tokens:

```plaintext
# Function codes
"\x01"
"\x03"
"\x06"
"\x10"

# Protocol ID
"\x00\x00"

# Common addresses
"\x00\x00"
"\x00\x01"
"\xFF\xFF"
```

Use it:
```bash
./fuzz_pdu_parser -dict=modbus.dict corpus/
```

---

## Coverage Analysis

### Line Coverage Report

```bash
# Build with coverage instrumentation
cmake -B build-cov \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_C_FLAGS="-fprofile-instr-generate -fcoverage-mapping" \
  -DENABLE_FUZZING=ON

cmake --build build-cov --target fuzz_mbap_decoder

# Run fuzzer to collect coverage
cd build-cov/tests/fuzz
LLVM_PROFILE_FILE="fuzzer.profraw" ./fuzz_mbap_decoder -runs=1000000 corpus/

# Generate coverage report
llvm-profdata merge -sparse fuzzer.profraw -o fuzzer.profdata
llvm-cov show ./fuzz_mbap_decoder \
  -instr-profile=fuzzer.profdata \
  -format=html \
  -output-dir=coverage_html \
  -show-line-counts-or-regions \
  -show-instantiation-summary

# View in browser
open coverage_html/index.html
```

### Coverage Summary

```bash
# Text summary
llvm-cov report ./fuzz_mbap_decoder \
  -instr-profile=fuzzer.profdata

# Example output:
# Filename                     Regions    Missed Regions     Cover   Functions  Missed Functions  Executed
# -------------------------------------------------------------------------
# mbap.c                            45                 2    95.56%          12                 0   100.00%
# pdu.c                             67                 8    88.06%          18                 1    94.44%
# crc.c                             12                 0   100.00%           3                 0   100.00%
# -------------------------------------------------------------------------
# TOTAL                            124                10    91.94%          33                 1    96.97%
```

### Identify Uncovered Code

```bash
# Show uncovered lines
llvm-cov show ./fuzz_mbap_decoder \
  -instr-profile=fuzzer.profdata \
  -show-line-counts-or-regions \
  -Xdemangler=c++filt | grep "0|"

# Example output:
#   45|      0|    if (header == NULL || pdu == NULL) {
#   46|      0|        return MBC_STATUS_NULL_PTR;
#   47|      0|    }
```

**Action:** Add test cases to corpus to hit line 45-47 (null pointer checks).

---

## Interpreting Results

### Success Indicators

```
#1048576   DONE   cov: 127 ft: 312 corp: 42/1234b exec/s: 17476 rss: 32Mb
```

| Metric | Meaning | Good Value |
|--------|---------|------------|
| `cov` | Code coverage (edges hit) | Increasing over time |
| `ft` | Features discovered | Increasing (new code paths) |
| `corp` | Corpus size (interesting inputs) | ~10-50 files |
| `exec/s` | Executions per second | >1000 (depends on code complexity) |
| `rss` | Memory usage (RSS) | Stable (<100 MB) |

### Failure Detection

LibFuzzer saves crash-inducing inputs to files:

| File Pattern | Issue Type | Priority |
|--------------|------------|----------|
| `crash-<hash>` | Program crashed (SIGSEGV, SIGABRT) | 🔴 **Critical** |
| `leak-<hash>` | Memory leak detected | 🟡 **Medium** |
| `timeout-<hash>` | Input caused hang (exceeded timeout) | 🟠 **High** |
| `oom-<hash>` | Out-of-memory error | 🟡 **Medium** |

### Reproducing Crashes

```bash
# Run fuzzer on specific crash file
./fuzz_mbap_decoder crash-da39a3ee5e6b4b0d

# Expected output if crash reproduces:
# ==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x60200000eff4
# READ of size 1 at 0x60200000eff4 thread T0
#     #0 0x4a5f23 in mbc_mbap_decode src/protocol/mbap.c:78
#     #1 0x4a6d12 in LLVMFuzzerTestOneInput tests/fuzz/fuzz_mbap_decoder.c:45
```

### Debugging Crashes

#### With GDB

```bash
gdb --args ./fuzz_mbap_decoder crash-da39a3ee5e6b4b0d

# Inside GDB:
(gdb) run
# ... crash occurs ...
(gdb) bt        # Backtrace
(gdb) frame 0   # Inspect crash location
(gdb) print header
(gdb) x/16xb data  # Examine input data
```

#### Minimize Crash Input

```bash
# Reduce crash file to smallest input that reproduces bug
./fuzz_mbap_decoder -minimize_crash=1 crash-da39a3ee5e6b4b0d

# Output: minimized-crash (e.g., 7 bytes instead of 256)
```

#### Hexdump Analysis

```bash
hexdump -C crash-da39a3ee5e6b4b0d

# Example output:
# 00000000  00 01 00 00 00 ff 00                              |.......|
#           ^TID ^Proto ^Len ^Unit
#                      ^^^^
#                      Length 0xFF (255) but only 7 bytes provided!
```

### Root Cause Analysis

```c
// Example: Bug in MBAP decoder
mbc_status_t mbc_mbap_decode(const uint8_t* data, size_t size,
                              mbc_mbap_header_t* header, ...)
{
    // Read length field (bytes 4-5)
    uint16_t length = (data[4] << 8) | data[5];

    // BUG: No validation that 'size' is sufficient for 'length' bytes!
    memcpy(pdu_buffer, data + MBC_MBAP_HEADER_SIZE, length);
    //                                                ^^^^^^
    //                                                Heap overflow!
}
```

**Fix:**
```c
// Validate length before memcpy
if (size < MBC_MBAP_HEADER_SIZE + length) {
    return MBC_STATUS_MALFORMED_FRAME;
}
```

---

## CI/CD Integration

### GitHub Actions Workflow

ModbusCore includes automated fuzzing in `.github/workflows/fuzz.yml`:

```yaml
name: Fuzzing

on:
  push:
    branches: [main, dev]
  pull_request:
    branches: [main]
  schedule:
    # Weekly deep fuzzing (Sundays at 2 AM UTC)
    - cron: '0 2 * * 0'
  workflow_dispatch:
    inputs:
      duration_seconds:
        description: 'Fuzzing duration per target (seconds)'
        default: '300'

jobs:
  fuzz:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        target: [fuzz_mbap_decoder, fuzz_rtu_crc, fuzz_pdu_parser]

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: sudo apt-get install -y clang cmake python3

      - name: Build fuzzer
        run: |
          cmake -B build -DCMAKE_C_COMPILER=clang -DENABLE_FUZZING=ON
          cmake --build build --target ${{ matrix.target }}

      - name: Generate corpus
        run: |
          cd build/tests/fuzz
          ./generate_corpus
          mkdir corpus && mv *.bin corpus/

      - name: Run fuzzer
        run: |
          cd build/tests/fuzz
          timeout 60s ./${{ matrix.target }} -max_total_time=60 corpus/ || true

      - name: Check for crashes
        run: |
          cd build/tests/fuzz
          if ls crash-* 2>/dev/null; then
            echo "❌ Fuzzer found crashes!"
            exit 1
          fi
          echo "✅ No crashes detected"
```

### Fuzzing Schedules

| Trigger | Duration | Purpose |
|---------|----------|---------|
| **Push/PR** | 60 sec/target | Fast feedback (smoke test) |
| **Weekly** | 6 hours/target | Deep testing (21,600 seconds) |
| **Manual** | User-defined | Custom campaigns (release testing) |

### Running CI Locally

```bash
# Simulate CI environment
docker run --rm -v $(pwd):/work -w /work ubuntu:latest bash -c '
  apt-get update && apt-get install -y clang cmake python3
  cmake -B build -DCMAKE_C_COMPILER=clang -DENABLE_FUZZING=ON
  cmake --build build
  cd build/tests/fuzz
  ./generate_corpus
  mkdir corpus && mv *.bin corpus/
  timeout 60s ./fuzz_mbap_decoder -max_total_time=60 corpus/ || true
  if ls crash-* 2>/dev/null; then exit 1; fi
'
```

### Pre-Commit Hook

Fuzz for 10 seconds before every commit:

```bash
# .git/hooks/pre-commit
#!/bin/bash
if [ -d build/tests/fuzz ]; then
  cd build/tests/fuzz
  timeout 10s ./fuzz_mbap_decoder -max_total_time=10 corpus/ || true
  if ls crash-* 2>/dev/null; then
    echo "❌ Pre-commit fuzzing found a crash! Fix before committing."
    exit 1
  fi
fi
```

---

## Alternative Fuzzers

### AFL++ (American Fuzzy Lop)

AFL++ is a coverage-guided fuzzer with different mutation strategies:

```bash
# Install AFL++
git clone https://github.com/AFLplusplus/AFLplusplus
cd AFLplusplus
make
sudo make install

# Build with AFL++ instrumentation
cmake -B build \
  -DCMAKE_C_COMPILER=afl-clang-fast \
  -DCMAKE_C_FLAGS="-fsanitize=address" \
  -DBUILD_TESTING=OFF

cmake --build build --target modbuscore

# Create standalone test harness
cat > afl_mbap.c << 'EOF'
#include <modbuscore/protocol/mbap.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    FILE* f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* data = malloc(size);
    fread(data, 1, size, f);
    fclose(f);

    mbc_mbap_header_t header;
    const uint8_t* pdu;
    size_t pdu_len;
    mbc_mbap_decode(data, size, &header, &pdu, &pdu_len);

    free(data);
    return 0;
}
EOF

afl-clang-fast afl_mbap.c -o afl_mbap -Lbuild/modbus -lmodbuscore

# Run AFL++
mkdir afl_in afl_out
cp build/tests/fuzz/corpus/*.bin afl_in/
afl-fuzz -i afl_in -o afl_out -m none ./afl_mbap @@
```

**AFL++ Advantages:**
- Different mutation strategies (deterministic + havoc)
- Better dictionary support
- Fork server mode (faster execution)

### Honggfuzz

```bash
# Install Honggfuzz
git clone https://github.com/google/honggfuzz
cd honggfuzz && make

# Build with Honggfuzz
cmake -B build \
  -DCMAKE_C_COMPILER=hfuzz-clang \
  -DCMAKE_C_FLAGS="-fsanitize=address"

# Create harness (similar to AFL++)
# ... (same structure as AFL example)

# Run Honggfuzz
honggfuzz -i corpus/ -o crashes/ -- ./hfuzz_mbap ___FILE___
```

**Honggfuzz Advantages:**
- Feedback-driven (coverage + sanitizers)
- Hardware-assisted tracing (Intel PT)
- Built-in crash analysis

---

## Best Practices

### 1. Seed Corpus Quality

**Good Corpus:**
- Valid Modbus frames (all function codes)
- Edge cases (minimum/maximum lengths)
- Mixed valid/invalid data

**Generate Corpus:**
```bash
cd build/tests/fuzz
./generate_corpus

# Contents:
# mbap_fc03_read_holding.bin    - Read Holding Registers (FC03)
# mbap_fc06_write_single.bin    - Write Single Register (FC06)
# mbap_fc16_write_multiple.bin  - Write Multiple Registers (FC16)
# rtu_fc03_with_crc.bin         - RTU frame with valid CRC
# pdu_fc01_read_coils.bin       - Read Coils PDU
# ... (15 files total)
```

### 2. Iterative Fuzzing

```bash
# Day 1: Quick test
./fuzz_mbap_decoder -max_total_time=3600 corpus/

# Day 2: Continue with discovered corpus
./fuzz_mbap_decoder corpus/ # Reuses corpus/ directory

# Day 3: Merge corpora from multiple runs
./fuzz_mbap_decoder -merge=1 corpus_merged/ corpus/ corpus_day2/
```

### 3. Corpus Minimization

Reduce corpus size while maintaining coverage:

```bash
# Before: 500 files, 2 MB
ls corpus/ | wc -l   # 500

# Minimize
mkdir corpus_min
./fuzz_mbap_decoder -merge=1 corpus_min/ corpus/

# After: 50 files, 200 KB (same coverage!)
ls corpus_min/ | wc -l   # 50
```

### 4. Regression Testing

```bash
# After fixing a crash, add it to regression suite
cp crash-da39a3ee5e6b4b0d tests/fuzz/regression/crash_20240115_mbap.bin

# Update CMakeLists.txt to test it
add_test(NAME fuzz_regression_mbap
         COMMAND fuzz_mbap_decoder tests/fuzz/regression/crash_20240115_mbap.bin)
```

### 5. Differential Fuzzing

Compare behavior against reference implementation (libmodbus):

```c
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Test both implementations
    mbc_mbap_header_t mbc_header;
    uint8_t* mbc_pdu;
    size_t mbc_len;
    mbc_status_t mbc_status = mbc_mbap_decode(data, size, &mbc_header, &mbc_pdu, &mbc_len);

    modbus_t* libmod_ctx = modbus_new_tcp("127.0.0.1", 502);
    int libmod_status = modbus_receive(libmod_ctx, (uint8_t*)data);

    // Compare results (should be consistent)
    if ((mbc_status == MBC_STATUS_OK) != (libmod_status >= 0)) {
        abort(); // Behavioral difference detected!
    }

    modbus_free(libmod_ctx);
    return 0;
}
```

---

## Troubleshooting

### Low Execution Rate (<100 exec/s)

**Symptoms:**
```
#10000  pulse  cov: 45 ft: 67 corp: 12/987b lim: 260 exec/s: 89 rss: 41Mb
                                                      ^^^^ Too slow!
```

**Solutions:**
1. **Disable verbose logging in fuzzing builds:**
   ```c
   #ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
   #define MBC_LOG(...) fprintf(stderr, __VA_ARGS__)
   #else
   #define MBC_LOG(...) ((void)0)
   #endif
   ```

2. **Use `-O2` optimization:**
   ```bash
   cmake -B build -DCMAKE_C_FLAGS="-O2 -fsanitize=fuzzer,address"
   ```

3. **Parallelize:**
   ```bash
   ./fuzz_mbap_decoder -jobs=$(nproc) corpus/
   ```

### Fuzzer Not Finding Crashes

**Verification Test:**
Inject a deliberate bug to confirm fuzzer works:

```c
// In src/protocol/mbap.c
mbc_status_t mbc_mbap_decode(...)
{
    // DELIBERATE BUG FOR TESTING
    if (size == 42) {
        *(int*)0 = 0; // Null pointer dereference
    }

    // ... rest of function
}
```

Rebuild and run:
```bash
cmake --build build --target fuzz_mbap_decoder
./build/tests/fuzz/fuzz_mbap_decoder corpus/

# Should crash within seconds:
# ==12345==ERROR: AddressSanitizer: SEGV on unknown address 0x000000000000
```

If it **still doesn't crash**, check:
- Sanitizers enabled? (`-fsanitize=address,fuzzer`)
- Fuzzer executable format (should be `ELF 64-bit LSB executable`)

### Out of Memory

```bash
# Limit RSS usage
./fuzz_mbap_decoder -rss_limit_mb=512 corpus/

# Reduce corpus size
./fuzz_mbap_decoder -max_len=512 corpus/
```

### Slow Inputs (Hangs)

```bash
# Reduce timeout (default: 1200 seconds)
./fuzz_mbap_decoder -timeout=10 corpus/

# Report slow units
./fuzz_mbap_decoder -report_slow_units=100 corpus/
```

### macOS ARM64 Issues

LibFuzzer has known issues on Apple Silicon. **Use Docker:**

```bash
# Automated Docker fuzzing
./scripts/fuzz_in_docker.sh 300

# Manual Docker run
docker run --rm -v $(pwd):/work -w /work ubuntu:latest bash -c '
  apt-get update && apt-get install -y clang cmake python3
  cmake -B build -DCMAKE_C_COMPILER=clang -DENABLE_FUZZING=ON
  cmake --build build --target fuzz_mbap_decoder
  cd build/tests/fuzz
  ./generate_corpus && mkdir corpus && mv *.bin corpus/
  ./fuzz_mbap_decoder -max_total_time=300 corpus/
'
```

---

## Phase 7 Exit Criteria

To complete Phase 7 fuzzing requirements:

| Requirement | Status | Target |
|-------------|--------|--------|
| ✅ Implement harnesses | Complete | MBAP, RTU, PDU fuzzers |
| ✅ CI integration | Complete | Automated fuzzing in GitHub Actions |
| ⏳ 1B executions without crash | In Progress | Run extended campaigns |
| ⏳ Document findings | In Progress | Report bugs discovered & fixed |

### Tracking Progress

```bash
# Check cumulative execution count across all runs
grep "Done\|DONE" fuzz_*.log | awk '{sum += $2} END {printf "%.2f billion executions\n", sum/1000000000}'

# Example output:
# 1.23 billion executions
```

**Target:** 1,000,000,000+ executions across all three fuzzers (MBAP, RTU, PDU).

---

## Further Reading

- **[LibFuzzer Documentation](https://llvm.org/docs/LibFuzzer.html)** - Official LLVM fuzzing guide
- **[Fuzzing Best Practices](https://google.github.io/fuzzing/tutorial/)** - Google's fuzzing tutorial
- **[AFL++ Documentation](https://github.com/AFLplusplus/AFLplusplus)** - Alternative fuzzer
- **[OSS-Fuzz](https://github.com/google/oss-fuzz)** - Continuous fuzzing for open-source projects
- **[tests/fuzz/README.md](../../tests/fuzz/README.md)** - ModbusCore fuzzing infrastructure details

---

## Summary

| Aspect | Recommendation |
|--------|----------------|
| **Fuzzer** | LibFuzzer (default), AFL++ (alternative) |
| **Duration** | 60 sec (CI), 1+ hours (releases), continuous (security-critical) |
| **Coverage Target** | >90% line coverage of protocol parsers |
| **Execution Target** | 1 billion+ executions without crashes |
| **CI Integration** | Automated on push/PR + weekly deep testing |
| **Corpus Management** | Start with generated seeds, minimize periodically |

**Next Steps:**
1. Run quick smoke test: `./scripts/fuzz_in_docker.sh 60`
2. Review coverage gaps: `llvm-cov report ...`
3. Schedule extended campaign: `./fuzz_mbap_decoder -runs=1000000000 corpus/`
4. Integrate into release checklist (see [TROUBLESHOOTING.md](../TROUBLESHOOTING.md))

Fuzzing is an ongoing process—continuous fuzzing catches regressions and ensures long-term robustness. 🛡️
