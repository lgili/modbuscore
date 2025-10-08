# Modbus Benchmarks

Performance benchmarking suite for the Modbus C library.

## Quick Start

### Build

```bash
cmake -B build -DMODBUS_BUILD_BENCHMARKS=ON
cmake --build build
```

### Run

```bash
./build/benchmarks/modbus_benchmarks
```

### Export Results

```bash
./build/benchmarks/modbus_benchmarks --json results.json
```

## Benchmark Suites

### Baseline (Framework Overhead)
- `bench_noop` - Empty function (pure framework overhead)
- `bench_inline_noop` - Inline empty function
- `bench_memory_read` - Single memory read operation
- `bench_memory_write` - Single memory write operation
- `bench_function_call` - Function call overhead

**Purpose**: Establish baseline overhead for interpreting other benchmarks.

### Encoding (TODO - Phase 2)
- `bench_encode_fc03_1reg` - Encode FC03 Read Holding (1 register)
- `bench_encode_fc03_10regs` - Encode FC03 (10 registers)
- `bench_encode_fc03_100regs` - Encode FC03 (100 registers)
- `bench_encode_fc16_10regs` - Encode FC16 Write Multiple
- `bench_encode_fc05` - Encode FC05 Write Single Coil

### Decoding (TODO - Phase 2)
- `bench_decode_fc03_10regs` - Decode FC03 response
- `bench_decode_fc16` - Decode FC16 response
- `bench_parse_rtu_frame` - Complete RTU frame parsing

### CRC (TODO - Phase 2)
- `bench_crc16_16bytes` - CRC16 on 16-byte payload
- `bench_crc16_64bytes` - CRC16 on 64-byte payload
- `bench_crc16_256bytes` - CRC16 on 256-byte payload

### End-to-End (TODO - Phase 3)
- `bench_rtu_loopback` - RTU client→server→client round trip
- `bench_tcp_loopback` - TCP client→server→client round trip
- `bench_client_poll_idle` - Client poll() when idle
- `bench_server_poll_idle` - Server poll() when idle

## Interpreting Results

### Units
- **ns** (nanoseconds) - 1×10⁻⁹ seconds
- **µs** (microseconds) - 1×10⁻⁶ seconds
- **ms** (milliseconds) - 1×10⁻³ seconds

### Statistics
- **min** - Fastest iteration (best case)
- **max** - Slowest iteration (worst case, includes OS jitter)
- **avg** - Average across all iterations
- **p50** - Median (50th percentile)
- **p95** - 95th percentile (typical worst case)
- **p99** - 99th percentile (outliers)

**Recommendation**: Focus on **p95** for production capacity planning. Max values often include OS scheduler preemption.

### Example Output

```
=== Modbus Benchmarks ===
Platform: Linux
Architecture: x86_64
Compiler: GCC 11.4.0
Date: 2025-10-08 12:34:56

Baseline:
  bench_noop                    :      18 ns  [min: 15 ns, max: 450 ns, p95: 25 ns]  ✅ PASS
  bench_function_call           :      14 ns  [min: 12 ns, max: 380 ns, p95: 20 ns]  ✅ PASS

Encoding:
  bench_encode_fc03_10regs      :     312 ns  [min: 298 ns, max: 1.2 µs, p95: 345 ns]  ✅ PASS

=== Summary ===
Total benchmarks: 3
Passed: 3 ✅
Failed: 0

All performance budgets met! 🎉
```

## Performance Budgets

Benchmarks can specify a performance budget (maximum acceptable time). If exceeded, the benchmark **fails** and CI can block the PR.

Example:
```c
static mb_bench_t bench_encode_fc03 = {
    .name = "bench_encode_fc03_10regs",
    .run = run_function,
    .iterations = 10000,
    .warmup_iters = 100,
    .budget_ns = 500,  // ← Must complete in ≤500ns
};
```

If `avg_ns > budget_ns`, output shows:
```
bench_encode_fc03_10regs  :  678 ns  [...]  ❌ FAIL (budget: 500 ns)
```

## Platform Support

### Timing Implementation

| Platform       | Timing Source                | Precision |
|----------------|------------------------------|-----------|
| Linux          | `clock_gettime(MONOTONIC)`   | ~1 ns     |
| macOS          | `clock_gettime(MONOTONIC)`   | ~1 ns     |
| Windows        | `QueryPerformanceCounter`    | ~100 ns   |
| ARM Cortex-M   | DWT cycle counter            | CPU cycle |
| RISC-V         | `mcycle` CSR                 | CPU cycle |
| Generic fallback | `clock()`                  | ~1 µs     |

### Embedded Note

On embedded targets (Cortex-M, RISC-V), results are in **CPU cycles**, not nanoseconds. Convert using:

```c
uint64_t ns = mb_bench_cycles_to_ns(cycles, cpu_freq_hz);
```

Example (STM32 @ 72 MHz):
```c
// 1440 cycles @ 72 MHz = 20 µs
uint64_t ns = mb_bench_cycles_to_ns(1440, 72000000);
```

## Adding New Benchmarks

1. Create `bench_myfeature.c`:

```c
#include "bench_common.h"

static void bench_my_operation_run(void *user_data)
{
    // Code to benchmark
    my_operation();
}

void bench_myfeature_register(void)
{
    static mb_bench_t bench_my_operation = {
        .name = "bench_my_operation",
        .run = bench_my_operation_run,
        .iterations = 10000,
        .warmup_iters = 100,
        .budget_ns = 1000,  // 1µs budget
    };
    mb_bench_register(&bench_my_operation);
}
```

2. Add to `CMakeLists.txt`:
```cmake
add_executable(modbus_benchmarks
    ...
    bench_myfeature.c
)
```

3. Register in `main_bench.c`:
```c
extern void bench_myfeature_register(void);

int main() {
    ...
    bench_myfeature_register();
    ...
}
```

## CI Integration (TODO - Phase 4)

Planned CI workflow:

```yaml
- name: Run benchmarks
  run: |
    cmake -B build -DMODBUS_BUILD_BENCHMARKS=ON
    cmake --build build --target modbus_benchmarks
    ./build/benchmarks/modbus_benchmarks --json results.json

- name: Check performance regression
  run: |
    python3 scripts/ci/check_performance_regression.py \
      --baseline benchmarks/baseline/linux-x86_64.json \
      --current results.json \
      --fail-on-regression
```

## Tips for Accurate Benchmarking

### 1. CPU Frequency Scaling
Disable frequency scaling on Linux:
```bash
sudo cpupower frequency-set --governor performance
```

### 2. CPU Pinning
Pin to specific core:
```bash
taskset -c 0 ./modbus_benchmarks
```

### 3. Iterations
- **Too few**: Noisy results, unreliable statistics
- **Too many**: Takes forever, not practical for CI
- **Recommended**: 10k-100k for sub-microsecond operations, 1k for millisecond operations

### 4. Warmup
Always include warmup iterations (typically 100-1000) to:
- Populate CPU caches
- Eliminate branch predictor cold start
- Prime OS scheduler

### 5. Compiler Optimization
Benchmarks are built with `-O3` (maximum optimization) to match production. Results with `-O0` are meaningless.

## JSON Output Format

```json
{
  "benchmarks": [
    {
      "name": "bench_encode_fc03_10regs",
      "iterations": 10000,
      "min_ns": 298,
      "max_ns": 1200,
      "avg_ns": 312,
      "p50_ns": 308,
      "p95_ns": 345,
      "p99_ns": 456,
      "budget_ns": 500,
      "passed": true
    }
  ]
}
```

Use this for:
- Historical tracking (commit-over-time charts)
- Regression detection in CI
- Comparison between platforms/compilers

## CI Integration

### Automated Regression Detection

Benchmarks run automatically on every push/PR via GitHub Actions:

```yaml
# .github/workflows/benchmarks.yml
- Run full benchmark suite
- Compare against baseline (20% threshold)
- Post results as PR comment
- Fail build if regression detected
```

**See**: [../scripts/ci/README.md](../scripts/ci/README.md) for CI documentation

### Performance Tracking

**Check for regressions:**
```bash
python3 scripts/ci/check_performance_regression.py \
    results.json baseline.json --threshold 10
```

**Plot trends over time:**
```bash
python3 scripts/ci/plot_benchmarks.py \
    benchmarks/baseline/ trends.png
```

### Baseline Management

Baselines stored in `benchmarks/baseline/`:
```
baseline_2025-10-08.json  # Initial baseline
baseline_2025-10-15.json  # After optimization
baseline_2025-10-22.json  # After refactoring
```

**Update baseline after intentional changes:**
```bash
./build/benchmarks/modbus_benchmarks --json results.json
cp results.json benchmarks/baseline/baseline_$(date +%Y-%m-%d).json
git add benchmarks/baseline/
git commit -m "chore: update performance baseline"
```

## Implementation Status

- ✅ Phase 1: Foundation (portable timing, statistics, JSON export)
- ✅ Phase 2: CRC & Encoding benchmarks (16 total)
- ✅ Phase 3: Synthetic benchmarks (round-trips, memory, coils)
- ✅ Phase 4: CI integration (automated regression detection)

**Total**: 22 benchmarks, 100% passing, full CI automation

## Future Enhancements

- [ ] Multi-platform baselines (Linux/macOS/Windows)
- [ ] Embedded target benchmarks (STM32 DWT, ESP32, RISC-V)
- [ ] Performance badges in README
- [ ] HTML report generation with interactive charts
- [ ] Benchmark comparison dashboard

## License

Same as main Modbus library (MIT).
