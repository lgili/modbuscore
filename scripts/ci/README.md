# CI Scripts for Modbus Benchmarks

This directory contains scripts for automated benchmark tracking and performance regression detection in CI/CD pipelines.

## Scripts

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

- [../benchmarks/README.md](../../benchmarks/README.md) - Benchmark implementation details
- [../../GATE28_PLAN.md](../../GATE28_PLAN.md) - Performance benchmark roadmap
- [.github/workflows/benchmarks.yml](../../.github/workflows/benchmarks.yml) - CI workflow definition
