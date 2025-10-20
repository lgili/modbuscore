# macOS Fuzzing Issues

## Problem

LibFuzzer on macOS ARM64 (Apple Silicon) with LLVM/Clang 21+ has symbol resolution issues with `std::__1::__hash_memory` and related C++ stdlib functions when compiling C code.

### Error Message
```
Undefined symbols for architecture arm64:
  "std::__1::__hash_memory(void const*, unsigned long)", referenced from:
      ...
ld: symbol(s) not found for architecture arm64
```

## Root Cause

The LibFuzzer runtime (`libclang_rt.fuzzer_osx.a`) is compiled as C++ and requires C++ stdlib symbols. When linking pure C code, these symbols are not available because:
1. The C compiler doesn't automatically link `libc++`
2. The `-fsanitize=fuzzer` flag alone doesn't pull in the required C++ runtime on macOS ARM64

## Workarounds Attempted

1. ✗ Adding `-lc++` to linker flags - still missing symbols
2. ✗ Adding `-stdlib=libc++` - incompatible with C compiler
3. ✗ Compiling C files as C++ (`set_source_files_properties LANGUAGE CXX`) - still fails

## Solution

**Use Linux for fuzzing.** The GitHub Actions CI runs fuzzing on `ubuntu-latest` where LibFuzzer works correctly with C code.

### For Local Development on macOS

**Option 1:** Use the Docker Script (Recommended)

We provide a ready-to-use script that handles everything:

```bash
# Quick test (60 seconds, all fuzzers)
./scripts/fuzz_in_docker.sh

# Custom duration (5 minutes)
./scripts/fuzz_in_docker.sh 300

# Specific fuzzer
./scripts/fuzz_in_docker.sh 60 fuzz_mbap_decoder

# Long campaign (1 hour)
./scripts/fuzz_in_docker.sh 3600
```

The script automatically:
- ✅ Builds Ubuntu container with all dependencies
- ✅ Compiles fuzzers inside container
- ✅ Generates seed corpus
- ✅ Runs fuzzing campaign
- ✅ Saves results to `fuzz_results_<timestamp>/`
- ✅ Detects crashes and generates reports

Requirements:
- Docker Desktop for Mac, **OR**
- Colima: `brew install colima docker && colima start`

**Option 2:** Manual Docker
```bash
docker run --rm -v $(pwd):/work -w /work ubuntu:24.04 bash -c '
  apt-get update && apt-get install -y clang cmake python3
  cmake -B build -DCMAKE_C_COMPILER=clang -DENABLE_FUZZING=ON
  cmake --build build
  cd build/tests/fuzz
  ./generate_corpus
  ./fuzz_mbap_decoder -max_total_time=60 corpus/
'
```

**Option 3:** Use Colima/Lima (native feel)
```bash
# Install colima
brew install colima

# Start Ubuntu VM
colima start --arch aarch64 --vm-type vz --runtime docker

# Run fuzzing in container
docker run -it --rm -v $(pwd):/work -w /work ubuntu:24.04 bash
# Then inside container: follow Docker Option 1 commands
```

**Option 4:** Wait for fix
Track LLVM issue: https://github.com/llvm/llvm-project/issues (search for "LibFuzzer macOS ARM64")

### For CI/CD

The `.github/workflows/fuzz.yml` workflow correctly uses `ubuntu-latest`, so fuzzing works automatically in CI.

## Alternative: AFL++ (TODO)

AFL++ has better macOS support and can be used as an alternative:

```bash
brew install afl++
# Compile with afl-clang-fast instead of clang
export CC=afl-clang-fast
cmake -B build-afl -DENABLE_FUZZING=OFF
cmake --build build-afl

# Run AFL++
afl-fuzz -i tests/fuzz/corpus -o findings -- ./build-afl/tests/...
```

*(AFL++ integration not yet implemented)*

## Status

- ✅ **Linux**: Fuzzing works perfectly
- ❌ **macOS ARM64**: Symbol resolution issues
- ⏳ **macOS x86_64**: Unknown (needs testing)
- ✅ **Windows**: Not tested (use WSL2 + Linux)

## References

- [LibFuzzer Documentation](https://llvm.org/docs/LibFuzzer.html)
- [LLVM Issue Tracker](https://github.com/llvm/llvm-project/issues)
- [AFL++ Homepage](https://github.com/AFLplusplus/AFLplusplus)
