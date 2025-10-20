# Fuzzing ModbusCore

This directory contains LibFuzzer-based fuzzing harnesses for ModbusCore protocol parsers and decoders.

## 🎯 Purpose

Fuzzing is a critical security testing technique that feeds random/mutated inputs to code to discover crashes, hangs, memory errors, and other undefined behavior. The goal is to achieve **1 billion+ executions without failure** to meet Phase 7 exit criteria.

## 📦 Fuzzing Targets

| Target | Description | Coverage |
|--------|-------------|----------|
| `fuzz_mbap_decoder` | MBAP (Modbus TCP) frame parsing | `mbc_mbap_decode()`, `mbc_mbap_expected_length()` |
| `fuzz_rtu_crc` | RTU CRC16 calculation & validation | `mbc_crc16()`, `mbc_crc16_validate()` |
| `fuzz_pdu_parser` | PDU encoding/decoding for all FCs | `mbc_pdu_encode/decode()`, FC03/06/16 parsers |

## 🛠️ Building

### Prerequisites

- **Clang** (required for LibFuzzer)
- **CMake** >= 3.20
- **Python 3** (for corpus generation)
- **Linux** (recommended - macOS ARM64 has known LibFuzzer issues, see `MACOS_ISSUES.md`)

### Build Commands

**On Linux:**

```bash
# Configure with fuzzing enabled
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DENABLE_FUZZING=ON

# Build all fuzzing targets
cmake --build build --parallel

# Build specific target
cmake --build build --target fuzz_mbap_decoder
```

**On macOS (using Docker):**

```bash
# Quick test (60 seconds)
./scripts/fuzz_in_docker.sh

# Custom duration (5 minutes)
./scripts/fuzz_in_docker.sh 300

# Specific fuzzer only
./scripts/fuzz_in_docker.sh 60 fuzz_mbap_decoder

# Long campaign (1 hour)
./scripts/fuzz_in_docker.sh 3600
```

The Docker script automatically:
- ✅ Builds Ubuntu container with dependencies
- ✅ Configures and compiles fuzzers
- ✅ Generates seed corpus
- ✅ Runs fuzzing campaign
- ✅ Saves results to `fuzz_results_<timestamp>/`
- ✅ Detects and archives crashes

### Generate Seed Corpus

The seed corpus provides valid Modbus frames to bootstrap fuzzing:

```bash
cd build/tests/fuzz
make generate_fuzz_corpus  # Or: cmake --build . --target generate_fuzz_corpus
ls -lh corpus/
```

This creates 15 binary files:
- 5 MBAP frames (TCP)
- 4 RTU frames (with CRC)
- 6 PDU payloads

## 🚀 Running Fuzzers

### Quick Test (CI-friendly)

```bash
cd build/tests/fuzz

# Run for 60 seconds with basic sanitizers
./fuzz_mbap_decoder -max_total_time=60 -print_final_stats=1 corpus/
```

### Extended Run (1B executions target)

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

### Parallel Fuzzing (Multi-core)

```bash
# Use all CPU cores for faster fuzzing
./fuzz_mbap_decoder -jobs=$(nproc) -workers=$(nproc) corpus/
```

### Custom Options

```bash
# Detailed logging
./fuzz_pdu_parser -verbosity=1 -print_pcs=1 corpus/

# Focus on specific input size range
./fuzz_rtu_crc -min_len=4 -max_len=256 corpus/

# Detect slow inputs (>100ms)
./fuzz_mbap_decoder -report_slow_units=100 corpus/
```

## 🔍 Interpreting Results

### Success Indicators

```
#1048576   DONE   cov: 127 ft: 312 corp: 42/1234b exec/s: 17476 rss: 32Mb
```

- `cov`: Code coverage (edges/blocks hit)
- `ft`: Features discovered
- `corp`: Corpus size (inputs that increase coverage)
- `exec/s`: Executions per second
- `rss`: Memory usage

### Failure Detection

LibFuzzer saves crash-inducing inputs to files:

- `crash-<hash>`: Program crashed (SIGSEGV, SIGABRT, assertion)
- `leak-<hash>`: Memory leak detected
- `timeout-<hash>`: Input caused hang (exceeded timeout)
- `oom-<hash>`: Out-of-memory error

### Reproducing Crashes

```bash
# Run fuzzer on specific crash file
./fuzz_mbap_decoder crash-da39a3ee5e6b4b0d

# Run under GDB
gdb --args ./fuzz_mbap_decoder crash-da39a3ee5e6b4b0d

# Minimize crash input
./fuzz_mbap_decoder -minimize_crash=1 crash-da39a3ee5e6b4b0d
```

## 🤖 CI/CD Integration

### GitHub Actions Workflow

Fuzzing runs automatically on:
- **Push/PR**: 60-second smoke test per target (fast feedback)
- **Weekly schedule**: 6-hour extended run (deep testing)
- **Manual trigger**: Configurable duration

See `.github/workflows/fuzz.yml` for details.

### Running CI Locally

```bash
# Simulate CI environment
docker run --rm -v $(pwd):/work -w /work ubuntu:latest bash -c '
  apt-get update && apt-get install -y clang cmake python3
  cmake -B build -DCMAKE_C_COMPILER=clang -DENABLE_FUZZING=ON
  cmake --build build
  cd build/tests/fuzz
  ./generate_corpus
  timeout 60s ./fuzz_mbap_decoder -max_total_time=60 corpus/ || true
  ls crash-* && exit 1 || exit 0
'
```

## 📊 Coverage Analysis

Generate coverage reports to identify untested code paths:

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
  -output-dir=coverage_html

# Open in browser
open coverage_html/index.html
```

## 🐛 Debugging Tips

### AddressSanitizer (ASan)

Already enabled in fuzzing builds. Detects:
- Buffer overflows
- Use-after-free
- Double-free
- Memory leaks

### UndefinedBehaviorSanitizer (UBSan)

Also enabled. Detects:
- Integer overflows
- Null pointer dereference
- Misaligned memory access

### Reproducing Issues Without Sanitizers

```bash
# Build release version without instrumentation
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target modbuscore

# Test crash on release build
./build-release/tests/test_mbap_standalone < crash-da39a3ee5e6b4b0d
```

## 📈 Phase 7 Exit Criteria

To complete Phase 7 fuzzing requirements:

1. ✅ **Implement harnesses**: MBAP, RTU, PDU parsers
2. ✅ **CI integration**: Automated fuzzing in GitHub Actions
3. ⏳ **1B executions without crash**: Run extended fuzzing campaigns
4. ⏳ **Document findings**: Report any bugs discovered and fixed

### Tracking Progress

```bash
# Check cumulative execution count across all runs
grep "Done " fuzz-*.log | awk '{sum += $2} END {print sum " total executions"}'
```

Target: **1,000,000,000+ executions** without crashes across all three fuzzers.

## 🔧 Troubleshooting

### Fuzzer Not Finding Crashes?

Good news! This means your code is robust. To verify fuzzer is working:

```bash
# Inject a deliberate bug for testing
echo "if (size == 42) { *(int*)0 = 0; }" >> src/protocol/mbap.c

# Rebuild and run - should crash within seconds
cmake --build build --target fuzz_mbap_decoder
./build/tests/fuzz/fuzz_mbap_decoder corpus/
```

### Low Execution Rate?

- Use `-jobs=$(nproc)` for parallel fuzzing
- Reduce sanitizer overhead (use `-O2` instead of `-O0`)
- Disable logging in production builds

### Out of Memory?

```bash
# Limit RSS usage
./fuzz_mbap_decoder -rss_limit_mb=512 corpus/
```

## 📚 Further Reading

- [LibFuzzer Documentation](https://llvm.org/docs/LibFuzzer.html)
- [Fuzzing Best Practices](https://google.github.io/fuzzing/tutorial/)
- [AFL++ (alternative fuzzer)](https://github.com/AFLplusplus/AFLplusplus)

## 📞 Support

Issues with fuzzing? Open a GitHub issue with:
- Crash file (attach `crash-*`)
- Fuzzer command line
- Build configuration (`cmake -LA build | grep ENABLE`)
- System info (`uname -a`, `clang --version`)
