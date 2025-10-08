#!/usr/bin/env python3
"""
Compare benchmark results against baseline and detect performance regressions.

Usage:
    python scripts/ci/check_performance_regression.py <current.json> <baseline.json> [--threshold 10]

Exit codes:
    0 - All benchmarks within acceptable range
    1 - Performance regression detected
    2 - Error (missing files, parse errors, etc.)
"""

import json
import sys
import argparse
from typing import Dict, List, Tuple


def load_benchmark_results(filepath: str) -> Dict:
    """Load benchmark results from JSON file."""
    try:
        with open(filepath, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"❌ Error: File not found: {filepath}", file=sys.stderr)
        sys.exit(2)
    except json.JSONDecodeError as e:
        print(f"❌ Error: Invalid JSON in {filepath}: {e}", file=sys.stderr)
        sys.exit(2)


def compare_benchmarks(current: Dict, baseline: Dict, threshold_percent: float) -> Tuple[bool, List[Dict]]:
    """
    Compare current benchmarks against baseline.
    
    Returns:
        (all_passed, regressions) where:
        - all_passed: True if no regressions detected
        - regressions: List of benchmark regression details
    """
    regressions = []
    
    current_benchmarks = {b['name']: b for b in current.get('benchmarks', [])}
    baseline_benchmarks = {b['name']: b for b in baseline.get('benchmarks', [])}
    
    for name, current_bench in current_benchmarks.items():
        if name not in baseline_benchmarks:
            print(f"⚠️  New benchmark: {name} (no baseline)")
            continue
        
        baseline_bench = baseline_benchmarks[name]
        
        current_avg = current_bench['avg_ns']
        baseline_avg = baseline_bench['avg_ns']
        
        if baseline_avg == 0:
            continue  # Avoid division by zero
        
        percent_change = ((current_avg - baseline_avg) / baseline_avg) * 100.0
        
        if percent_change > threshold_percent:
            regressions.append({
                'name': name,
                'baseline_avg': baseline_avg,
                'current_avg': current_avg,
                'percent_change': percent_change,
                'threshold': threshold_percent
            })
    
    return len(regressions) == 0, regressions


def format_time(ns: float) -> str:
    """Format nanoseconds into human-readable string."""
    if ns < 1000:
        return f"{ns:.0f} ns"
    elif ns < 1_000_000:
        return f"{ns / 1000:.2f} µs"
    elif ns < 1_000_000_000:
        return f"{ns / 1_000_000:.2f} ms"
    else:
        return f"{ns / 1_000_000_000:.2f} s"


def print_report(current: Dict, baseline: Dict, regressions: List[Dict]):
    """Print detailed comparison report."""
    print("\n" + "=" * 80)
    print("📊 Benchmark Performance Report")
    print("=" * 80)
    
    current_benchmarks = {b['name']: b for b in current.get('benchmarks', [])}
    baseline_benchmarks = {b['name']: b for b in baseline.get('benchmarks', [])}
    
    print(f"\nTotal benchmarks: {len(current_benchmarks)}")
    print(f"Regressions detected: {len(regressions)}")
    
    if regressions:
        print("\n⚠️  PERFORMANCE REGRESSIONS DETECTED:")
        print("-" * 80)
        print(f"{'Benchmark':<30} {'Baseline':<12} {'Current':<12} {'Change':>10}")
        print("-" * 80)
        
        for reg in regressions:
            name = reg['name']
            baseline_str = format_time(reg['baseline_avg'])
            current_str = format_time(reg['current_avg'])
            change_str = f"+{reg['percent_change']:.1f}%"
            
            print(f"{name:<30} {baseline_str:<12} {current_str:<12} {change_str:>10} ❌")
    else:
        print("\n✅ All benchmarks within acceptable performance range!")
    
    # Show improvements
    improvements = []
    for name, current_bench in current_benchmarks.items():
        if name not in baseline_benchmarks:
            continue
        
        baseline_bench = baseline_benchmarks[name]
        current_avg = current_bench['avg_ns']
        baseline_avg = baseline_bench['avg_ns']
        
        if baseline_avg == 0:
            continue
        
        percent_change = ((current_avg - baseline_avg) / baseline_avg) * 100.0
        
        if percent_change < -5.0:  # More than 5% improvement
            improvements.append({
                'name': name,
                'baseline_avg': baseline_avg,
                'current_avg': current_avg,
                'percent_change': percent_change
            })
    
    if improvements:
        print("\n🚀 Performance Improvements:")
        print("-" * 80)
        print(f"{'Benchmark':<30} {'Baseline':<12} {'Current':<12} {'Change':>10}")
        print("-" * 80)
        
        for imp in improvements:
            name = imp['name']
            baseline_str = format_time(imp['baseline_avg'])
            current_str = format_time(imp['current_avg'])
            change_str = f"{imp['percent_change']:.1f}%"
            
            print(f"{name:<30} {baseline_str:<12} {current_str:<12} {change_str:>10} ✅")
    
    print("\n" + "=" * 80)


def main():
    parser = argparse.ArgumentParser(
        description='Compare benchmark results and detect performance regressions'
    )
    parser.add_argument('current', help='Path to current benchmark results JSON')
    parser.add_argument('baseline', help='Path to baseline benchmark results JSON')
    parser.add_argument(
        '--threshold',
        type=float,
        default=10.0,
        help='Regression threshold in percent (default: 10%%)'
    )
    
    args = parser.parse_args()
    
    current = load_benchmark_results(args.current)
    baseline = load_benchmark_results(args.baseline)
    
    all_passed, regressions = compare_benchmarks(current, baseline, args.threshold)
    
    print_report(current, baseline, regressions)
    
    if not all_passed:
        print(f"\n❌ Performance regression detected (threshold: {args.threshold}%)")
        print(f"   {len(regressions)} benchmark(s) regressed beyond acceptable limits.")
        sys.exit(1)
    else:
        print(f"\n✅ No performance regressions detected (threshold: {args.threshold}%)")
        sys.exit(0)


if __name__ == '__main__':
    main()
