# Benchmark Baselines

This directory contains timestamped performance baselines for regression tracking.

## File Naming Convention

```
baseline_YYYY-MM-DD.json
```

Example: `baseline_2025-10-08.json`

## Purpose

Baselines serve as reference points for detecting performance regressions in CI. When benchmarks are run, the results are compared against the most recent baseline file.

## When to Update

Update the baseline after:

1. **Intentional performance optimizations** - Document the improvement
2. **Major refactoring** that affects performance characteristics
3. **Platform/compiler upgrades** that change performance baseline
4. **New benchmarks added** - Establish initial baseline

## How to Update

```bash
# Run benchmarks and generate JSON
./build/benchmarks/modbus_benchmarks --json results.json

# Copy as new baseline with today's date
cp results.json benchmarks/baseline/baseline_$(date +%Y-%m-%d).json

# Commit to git
git add benchmarks/baseline/baseline_*.json
git commit -m "chore: update performance baseline after [reason]"
git push
```

## Baseline History

| Date | Platform | Compiler | Notes |
|------|----------|----------|-------|
| 2025-10-08 | macOS ARM64 | Clang 17.0.0 | Initial baseline - Gate 28 Phase 3 complete |

## Current Baseline

**Latest**: `baseline_2025-10-08.json`

**Benchmarks**: 22 total
- 5 baseline (framework overhead)
- 3 CRC16 (16/64/256 bytes)
- 8 encoding (FC03/FC16/FC05/FC06)
- 6 synthetic (round-trips, memory, coils)

**Key Metrics** (macOS ARM64):
- Framework overhead: ~32ns
- CRC16 (256B): 11.88µs (45+ MB/s)
- FC03 encoding: 36-38ns
- FC16 encoding (100 regs): 437-462ns
- FC03 round-trip: 41-42ns

## Regression Detection

CI uses the most recent baseline file automatically. Threshold is set to **20%** for CI environments to account for VM variability.

To test locally:
```bash
python3 scripts/ci/check_performance_regression.py \
    results.json \
    benchmarks/baseline/baseline_2025-10-08.json \
    --threshold 10
```

## See Also

- [../README.md](../README.md) - Benchmark implementation details
- [../../scripts/ci/README.md](../../scripts/ci/README.md) - CI scripts documentation
- [../../GATE28_PLAN.md](../../GATE28_PLAN.md) - Performance benchmark roadmap
