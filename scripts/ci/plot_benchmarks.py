#!/usr/bin/env python3
"""
Plot benchmark performance trends over time.

Usage:
    python scripts/ci/plot_benchmarks.py <results_dir> <output.png>

This script scans a directory of JSON benchmark results and generates
line plots showing performance trends for each benchmark over time.
"""

import json
import sys
import argparse
from pathlib import Path
from typing import Dict, List, Tuple
from datetime import datetime


def load_all_results(results_dir: Path) -> List[Tuple[datetime, Dict]]:
    """
    Load all benchmark results from directory.
    
    Expected filename format: benchmark_YYYY-MM-DD_HH-MM-SS.json
    
    Returns:
        List of (timestamp, results_dict) tuples, sorted by timestamp
    """
    results = []
    
    for filepath in results_dir.glob("benchmark_*.json"):
        try:
            # Parse timestamp from filename
            timestamp_str = filepath.stem.replace("benchmark_", "")
            timestamp = datetime.strptime(timestamp_str, "%Y-%m-%d_%H-%M-%S")
            
            with open(filepath, 'r') as f:
                data = json.load(f)
            
            results.append((timestamp, data))
        except (ValueError, json.JSONDecodeError) as e:
            print(f"⚠️  Skipping {filepath.name}: {e}", file=sys.stderr)
            continue
    
    results.sort(key=lambda x: x[0])
    return results


def extract_benchmark_series(results: List[Tuple[datetime, Dict]]) -> Dict[str, List[Tuple[datetime, float]]]:
    """
    Extract time series data for each benchmark.
    
    Returns:
        Dict mapping benchmark name to list of (timestamp, avg_ns) points
    """
    series = {}
    
    for timestamp, data in results:
        for bench in data.get('benchmarks', []):
            name = bench['name']
            avg_ns = bench['avg_ns']
            
            if name not in series:
                series[name] = []
            
            series[name].append((timestamp, avg_ns))
    
    return series


def plot_benchmarks(series: Dict[str, List[Tuple[datetime, float]]], output_path: Path):
    """Generate plots using matplotlib."""
    try:
        import matplotlib.pyplot as plt
        import matplotlib.dates as mdates
    except ImportError:
        print("❌ Error: matplotlib not installed. Install with: pip install matplotlib", file=sys.stderr)
        sys.exit(2)
    
    if not series:
        print("❌ Error: No benchmark data to plot", file=sys.stderr)
        sys.exit(2)
    
    # Group benchmarks by category (based on name prefix)
    categories = {}
    for name, data in series.items():
        prefix = name.split('_')[1] if '_' in name else 'other'
        if prefix not in categories:
            categories[prefix] = {}
        categories[prefix][name] = data
    
    num_categories = len(categories)
    fig, axes = plt.subplots(num_categories, 1, figsize=(12, 4 * num_categories))
    
    if num_categories == 1:
        axes = [axes]
    
    for idx, (category, benchmarks) in enumerate(sorted(categories.items())):
        ax = axes[idx]
        
        for name, data in sorted(benchmarks.items()):
            timestamps, values = zip(*data)
            
            # Convert ns to appropriate unit
            if max(values) < 1000:
                values_scaled = values
                unit = "ns"
            elif max(values) < 1_000_000:
                values_scaled = [v / 1000 for v in values]
                unit = "µs"
            else:
                values_scaled = [v / 1_000_000 for v in values]
                unit = "ms"
            
            ax.plot(timestamps, values_scaled, marker='o', label=name, linewidth=2)
        
        ax.set_xlabel("Date")
        ax.set_ylabel(f"Average Time ({unit})")
        ax.set_title(f"Benchmark Performance - {category.upper()}")
        ax.legend(loc='best', fontsize=8)
        ax.grid(True, alpha=0.3)
        
        # Format x-axis dates
        ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))
        ax.xaxis.set_major_locator(mdates.AutoDateLocator())
        fig.autofmt_xdate()
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"✅ Plot saved to {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Plot benchmark performance trends over time'
    )
    parser.add_argument('results_dir', help='Directory containing benchmark JSON files')
    parser.add_argument('output', help='Output PNG file path')
    
    args = parser.parse_args()
    
    results_dir = Path(args.results_dir)
    output_path = Path(args.output)
    
    if not results_dir.exists():
        print(f"❌ Error: Directory not found: {results_dir}", file=sys.stderr)
        sys.exit(2)
    
    print(f"📊 Loading benchmark results from {results_dir}...")
    results = load_all_results(results_dir)
    
    if not results:
        print(f"❌ Error: No benchmark results found in {results_dir}", file=sys.stderr)
        sys.exit(2)
    
    print(f"   Found {len(results)} result files")
    
    print("📈 Extracting time series data...")
    series = extract_benchmark_series(results)
    print(f"   Tracking {len(series)} benchmarks")
    
    print("🎨 Generating plots...")
    plot_benchmarks(series, output_path)


if __name__ == '__main__':
    main()
