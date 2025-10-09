# CI Scripts for Modbus Library

This directory contains scripts for automated testing, benchmarking, and resource analysis in CI/CD pipelines.

## Scripts Overview

| Script | Purpose | Status | Gate |
|--------|---------|--------|------|
| `check_performance_regression.py` | Detect benchmark regressions | ✅ Active | Gate 28 |
| `plot_benchmarks.py` | Visualize performance trends | ✅ Active | Gate 28 |
| `report_footprint.py` | Build + measure ROM/RAM (UNIFIED) | ✅ Active | Gate 17 + 30 |
| `measure_footprint.py` | Map file analysis (standalone) | ⚠️ Deprecated | Gate 30 |
| `size_report.py` | Lightweight reporter (skeleton) | ⚠️ Deprecated | Gate 30 |

**Note**: `report_footprint.py` is now the **single source of truth** for footprint analysis,
combining build automation (Gate 17) with advanced analysis (Gate 30).

---

## Performance Benchmarking (Gate 28)

### 📊 `check_performance_regression.py`

Compares current benchmark results against a baseline and detects performance regressions.

**Usage:**
```bash
python scripts/ci/check_performance_regression.py <current.json> <baseline.json> [--threshold 10]
```

**Arguments:**
- `current.json`: Path to current benchmark results
- `baseline.json`: Path to baseline benchmark results
- `--threshold`: Regression threshold in percent (default: 10%)

**Exit Codes:**
- `0`: All benchmarks within acceptable range
- `1`: Performance regression detected
- `2`: Error (missing files, parse errors, etc.)

**Example:**
```bash
# Compare against baseline with 15% threshold
python scripts/ci/check_performance_regression.py \
    build/benchmarks/results.json \
    benchmarks/baseline/baseline_2025-10-08.json \
    --threshold 15

# Output:
# ================================================================================
# 📊 Benchmark Performance Report
# ================================================================================
# Total benchmarks: 22
# Regressions detected: 0
# ✅ All benchmarks within acceptable performance range!
```

### 📈 `plot_benchmarks.py`

Generates line plots showing benchmark performance trends over time.

**Usage:**
```bash
python scripts/ci/plot_benchmarks.py <results_dir> <output.png>
```

**Arguments:**
- `results_dir`: Directory containing timestamped benchmark JSON files
- `output.png`: Output image file path

**Expected Filename Format:**
```
benchmark_YYYY-MM-DD_HH-MM-SS.json
```

**Example:**
```bash
# Generate trend plot from historical results
python scripts/ci/plot_benchmarks.py \
    benchmarks/baseline/ \
    docs/benchmark_trends.png
```

**Requirements:**
```bash
pip install matplotlib
```

## Integration with GitHub Actions

### Workflow Setup

The `.github/workflows/benchmarks.yml` workflow automatically:

1. **Builds** the benchmark suite with optimizations (`-O3`)
2. **Runs** all benchmarks and exports results to JSON
3. **Uploads** results as artifacts for download
4. **Compares** against baseline (if available)
5. **Fails** the build if regressions exceed threshold

### Baseline Management

Baselines are stored in `benchmarks/baseline/` with timestamps:

```
benchmarks/baseline/
├── baseline_2025-10-08.json
├── baseline_2025-10-15.json
└── baseline_2025-10-22.json
```

**To update the baseline:**
```bash
# Run benchmarks
./build/benchmarks/modbus_benchmarks --json results.json

# Copy as new baseline
cp results.json benchmarks/baseline/baseline_$(date +%Y-%m-%d).json

# Commit to git
git add benchmarks/baseline/baseline_*.json
git commit -m "chore: update performance baseline"
```

## Benchmark Result Format

JSON schema for benchmark results:

```json
{
  "benchmarks": [
    {
      "name": "bench_crc16_256bytes",
      "iterations": 100000,
      "min_ns": 5000,
      "max_ns": 44000,
      "avg_ns": 5600,
      "p50_ns": 5000,
      "p95_ns": 7000,
      "p99_ns": 8000,
      "budget_ns": 50000,
      "passed": true
    }
  ]
}
```

## Performance Thresholds

Recommended thresholds by environment:

| Environment | Threshold | Rationale |
|-------------|-----------|-----------|
| **CI (Ubuntu VM)** | 15-20% | Higher variability in cloud VMs |
| **Local Dev** | 10% | More stable environment |
| **Dedicated Hardware** | 5% | Minimal noise, strict requirements |

### Why Different Thresholds?

- **CI environments** have shared resources, thermal throttling, and background processes
- **Local development** machines are more consistent but still vary
- **Dedicated hardware** (e.g., embedded targets) should be very consistent

## Troubleshooting

### False Positives (Spurious Regressions)

If you see random regressions in CI:

1. **Increase threshold**: Try 15-20% for noisy environments
2. **Multiple runs**: Average 3-5 benchmark runs
3. **Disable turbo boost**: For consistent clock speeds
4. **Pin CPU affinity**: Lock benchmarks to specific cores

```bash
# Example: Run 3 times and average
for i in {1..3}; do
  ./benchmarks/modbus_benchmarks --json results_$i.json
done
python scripts/ci/average_results.py results_*.json > results_avg.json
```

### Missing Baseline

If no baseline exists yet:

```bash
# First run establishes baseline
./benchmarks/modbus_benchmarks --json baseline.json
cp baseline.json benchmarks/baseline/baseline_$(date +%Y-%m-%d).json
git add benchmarks/baseline/
git commit -m "chore: establish initial performance baseline"
```

## Best Practices

1. **Update baselines** after intentional performance changes
2. **Document changes** in commit messages when updating baselines
3. **Review trends** regularly using `plot_benchmarks.py`
4. **Investigate regressions** immediately - they compound over time
5. **Use realistic data** in benchmarks (not all zeros/patterns)

## See Also

- [../../benchmarks/README.md](../../benchmarks/README.md) - Benchmark implementation details
- [../../docs/FOOTPRINT.md](../../docs/FOOTPRINT.md) - ROM/RAM footprint guide (Gate 30)
- [../../docs/STACK_ANALYSIS.md](../../docs/STACK_ANALYSIS.md) - Stack usage analysis (Gate 30)
- [../../docs/RESOURCE_PLANNING.md](../../docs/RESOURCE_PLANNING.md) - Resource planning guide (Gate 30)
- [../../GATE28_PLAN.md](../../GATE28_PLAN.md) - Performance benchmark roadmap
- [../../GATE30_PLAN.md](../../GATE30_PLAN.md) - Footprint & stack analysis roadmap
- [../../.github/workflows/benchmarks.yml](../../.github/workflows/benchmarks.yml) - CI workflow definition

---

## Footprint & Stack Analysis (Gate 17 + Gate 30 - UNIFIED)

### 📏 `report_footprint.py` - Unified Footprint Analysis

**The single tool for all footprint needs** - combines build automation with advanced analysis.

**Features** (Gate 17 + Gate 30):
- ✅ **Build automation**: Configure + compile for multiple profiles/targets
- ✅ **Multi-target support**: host, stm32g0, esp32c3 (ARM, RISC-V)
- ✅ **Size tool integration**: llvm-size, arm-none-eabi-size, etc.
- ✅ **Map file parsing**: Detailed .text/.rodata/.data/.bss breakdown (Gate 30)
- ✅ **Baseline comparison**: Detect footprint regressions (Gate 30)
- ✅ **CI integration**: Exit code 1 on regression (Gate 30)
- ✅ **README.md integration**: Auto-update footprint tables (Gate 17)
- ✅ **JSON output**: Structured metrics for downstream tools

**Quick Start**:
```bash
# Build and measure (default: all profiles, all targets)
make footprint

# CI mode: Check against baseline
make footprint-check

# Custom targets
python3 scripts/ci/report_footprint.py \
  --profiles TINY LEAN FULL \
  --targets host stm32g0 \
  --output metrics.json \
  --update-readme README.md
```

**Gate 30 Features** (Advanced):
```bash
# Check against baseline with custom threshold
python3 scripts/ci/report_footprint.py \
  --profiles TINY LEAN FULL \
  --targets host \
  --baseline scripts/ci/footprint_baseline.json \
  --threshold 0.05 \
  --output current.json

# Use map files for detailed analysis (if available)
python3 scripts/ci/report_footprint.py \
  --profiles TINY LEAN FULL \
  --targets stm32g0 \
  --use-map-files \
  --output detailed.json

# Save current as new baseline
python3 scripts/ci/report_footprint.py \
  --profiles TINY LEAN FULL \
  --targets host \
  --save-baseline scripts/ci/footprint_baseline.json
```

**Output Example**:
```
Target     Profile  ROM (archive)  ROM (objects)  RAM (objects)  Artifact
--------------------------------------------------------------------------
host       TINY           6400          6144           512       build/.../libmodbus.a
host       LEAN          11264         10752          1280       build/.../libmodbus.a
host       FULL          20480         19456          3584       build/.../libmodbus.a

✅ All footprint checks passed!
```

**Baseline Comparison** (when using `--baseline`):
```
## Footprint Comparison vs Baseline

| Target | Profile | Metric | Baseline | Current | Change | Status |
|--------|---------|--------|----------|---------|--------|--------|
| host   | TINY    | ROM    | 6.2 KB   | 6.4 KB  | +3.2%  | ✅     |
| host   | TINY    | RAM    | 512 B    | 512 B   | +0.0%  | ✅     |
| host   | LEAN    | ROM    | 11.0 KB  | 11.3 KB | +2.7%  | ✅     |
| host   | LEAN    | RAM    | 1.2 KB   | 1.3 KB  | +8.3%  | ❌     |

❌ Footprint regression detected!
```

**Migration from Old Scripts**:
```bash
# Old (measure_footprint.py):
python3 scripts/ci/measure_footprint.py --all --baseline baseline.json

# New (report_footprint.py):
python3 scripts/ci/report_footprint.py \
  --profiles TINY LEAN FULL \
  --targets host \
  --baseline baseline.json \
  --threshold 0.05

# Or simply:
make footprint-check
```

---

### ⚠️ Deprecated Scripts

#### `measure_footprint.py` (Gate 30) - DEPRECATED
**Status**: Merged into `report_footprint.py`

This standalone script provided map file parsing and baseline comparison but
lacked build automation. All functionality has been integrated into
`report_footprint.py`. Use `make footprint-check` instead.

**Migration**: See `report_footprint.py` usage above.

#### `size_report.py` (Gate 30) - DEPRECATED
**Status**: Replaced by `report_footprint.py`

This was a lightweight skeleton for future CI work. It has been replaced by
the enhanced `report_footprint.py`. File will be removed in next major release.

---

### 📐 `analyze_stack.py` - Stack Usage Analysis

Parse GCC `.su` files to compute worst-case stack usage for critical execution paths.

**Features**:
- Parse `.text`, `.rodata`, `.data`, `.bss` sections
- Support multiple profiles (TINY/LEAN/FULL)
- Support multiple MCU targets (Cortex-M0+/M4F/M33/RISC-V)
- Generate Markdown tables (detailed + summary)
- Compare against baseline JSON
- CI integration (fail on regression)

**Usage:**
```bash
# Analyze single map file
python scripts/ci/measure_footprint.py \
  --map build/footprint/cortex-m0plus/TINY.map

# Analyze all profiles for one target
python scripts/ci/measure_footprint.py --target cortex-m0plus

# Generate full report
python scripts/ci/measure_footprint.py --all --output report.md

# CI mode (fail if regression > 5%)
python scripts/ci/measure_footprint.py \
  --all \
  --baseline scripts/ci/footprint_baseline.json \
  --threshold 0.05
```

**Output Example:**
```
✅ Parsed TINY.map: ROM=6.2 KB, RAM=512 B
✅ Parsed LEAN.map: ROM=11.3 KB, RAM=1.3 KB

## Resource Usage Summary

| Profile | Cortex-M0+ ROM | Cortex-M0+ RAM | Cortex-M4F ROM | Cortex-M4F RAM |
|---------|----------------|----------------|----------------|----------------|
| TINY    | 6.2 KB         | 512 B          | 5.8 KB         | 512 B          |
| LEAN    | 11.4 KB        | 1.3 KB         | 10.8 KB        | 1.2 KB         |
| FULL    | 18.7 KB        | 3.5 KB         | 17.9 KB        | 3.4 KB         |
```

**Baseline Management:**
```bash
# Create baseline from current measurements
python scripts/ci/measure_footprint.py \
  --all \
  --save-baseline footprint_baseline.json

# Update baseline after feature addition
git add footprint_baseline.json
git commit -m "chore: update footprint baseline for feature X"
```

### 📐 `analyze_stack.py`

Parse GCC `.su` files to compute worst-case stack usage for critical execution paths.

**Features**:
- Parse `.su` files (generated with `-fstack-usage`)
- Build call graph from source code
- Compute worst-case stack depth per entry point
- Identify top stack consumers
- Critical path analysis with call trees
- Generate Markdown report with statistics

**Usage:**
```bash
# Analyze single .su file
python scripts/ci/analyze_stack.py \
  --su build/stack-usage/cortex-m0plus/modbus.c.su

# Analyze all .su files for one target
python scripts/ci/analyze_stack.py --target cortex-m0plus

# Worst-case for specific entry point
python scripts/ci/analyze_stack.py \
  --all \
  --critical-path mb_client_poll

# Generate report
python scripts/ci/analyze_stack.py \
  --all \
  --output stack_report.md
```

**Output Example:**
```
✅ Parsed modbus.c.su: 42 functions

📊 Worst-case stack for `mb_client_poll()`: 304 bytes

Call chain:
  └─ mb_client_poll: 96 bytes
     └─ mb_fsm_run: 64 bytes
        └─ mb_decode_response: 80 bytes
           └─ mb_validate_crc: 32 bytes
              └─ user_callback: 32 bytes

### Top 10 Stack Consumers

| Function | Stack (bytes) | Qualifier | File:Line |
|----------|---------------|-----------|-----------|
| `mb_encode_fc03_request` | 128 | static | pdu_encode.c:245 |
| `mb_decode_fc03_response` | 96 | static | pdu_decode.c:189 |
| `mb_client_poll` | 96 | static | client.c:1166 |
```

### Building for Footprint/Stack Analysis

**Generate Map File:**
```bash
# Configure for footprint analysis
cmake --preset host-footprint -DMODBUS_PROFILE=TINY

# Build with map file
cmake --build build/host-footprint

# Map file location: build/host-footprint/modbus/libmodbus.a.map
```

**Generate Stack Usage Files:**
```bash
# Configure with stack usage flags
cmake --preset host-footprint \
  -DCMAKE_C_FLAGS="-fstack-usage"

# Build
cmake --build build/host-footprint

# .su files location: build/host-footprint/**/*.su
```

### CI Integration Example

**GitHub Actions** (footprint regression detection):
```yaml
name: Footprint Check

on: [pull_request]

jobs:
  footprint:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install ARM GCC
        run: sudo apt install gcc-arm-none-eabi
      
      - name: Build
        run: |
          cmake --preset host-footprint
          cmake --build build/host-footprint
      
      - name: Measure Footprint
        run: |
          python3 scripts/ci/measure_footprint.py \
            --all \
            --baseline scripts/ci/footprint_baseline.json \
            --threshold 0.05 \
            --output footprint_report.md
      
      - name: Upload Report
        uses: actions/upload-artifact@v3
        with:
          name: footprint-report
          path: footprint_report.md
```

---
