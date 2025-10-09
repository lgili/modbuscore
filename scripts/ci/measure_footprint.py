#!/usr/bin/env python3
"""
⚠️ DEPRECATED: This script has been merged into report_footprint.py

This standalone script is kept for backward compatibility but will be removed
in a future release. Please use report_footprint.py instead:

Old:
    python3 scripts/ci/measure_footprint.py --all --baseline baseline.json

New:
    python3 scripts/ci/report_footprint.py \
        --profiles TINY LEAN FULL \
        --targets host \
        --baseline baseline.json \
        --threshold 0.05

Or simply:
    make footprint-check

The enhanced report_footprint.py provides all the functionality of this script
plus build automation and README.md integration.

---

Measure ROM/RAM footprint for different Modbus profiles and MCU targets.

This script:
1. Parses GCC map files to extract .text (ROM) and .data/.bss (RAM)
2. Supports multiple profiles (TINY, LEAN, FULL)
3. Supports multiple MCU targets (Cortex-M0+, Cortex-M4F)
4. Generates Markdown tables for documentation
5. Can be used in CI to detect footprint regressions

Usage:
    # Analyze single map file
    python3 scripts/ci/measure_footprint.py --map build/footprint/cortex-m0plus/TINY.map

    # Analyze all profiles for one target
    python3 scripts/ci/measure_footprint.py --target cortex-m0plus

    # Generate full report for all targets
    python3 scripts/ci/measure_footprint.py --all

    # Compare against baseline
    python3 scripts/ci/measure_footprint.py --all --baseline scripts/ci/footprint_baseline.json
"""

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# ⚠️ DEPRECATION WARNING - Show at runtime
print("=" * 70, file=sys.stderr)
print("⚠️  WARNING: measure_footprint.py is DEPRECATED", file=sys.stderr)
print("=" * 70, file=sys.stderr)
print("", file=sys.stderr)
print("This script has been merged into report_footprint.py.", file=sys.stderr)
print("Please use one of the following instead:", file=sys.stderr)
print("", file=sys.stderr)
print("  make footprint          # Full build + measure + README update", file=sys.stderr)
print("  make footprint-check    # CI mode with baseline comparison", file=sys.stderr)
print("", file=sys.stderr)
print("This script will be removed in a future release.", file=sys.stderr)
print("=" * 70, file=sys.stderr)
print("", file=sys.stderr)


@dataclass
class FootprintData:
    """Footprint measurements for a single build configuration."""
    profile: str          # TINY, LEAN, FULL
    target: str           # cortex-m0plus, cortex-m4f, etc.
    text: int             # ROM (code)
    rodata: int           # ROM (constants)
    data: int             # RAM (initialized)
    bss: int              # RAM (zero-initialized)

    @property
    def total_rom(self) -> int:
        """Total ROM = .text + .rodata"""
        return self.text + self.rodata

    @property
    def total_ram(self) -> int:
        """Total RAM = .data + .bss"""
        return self.data + self.bss


def parse_gcc_map_file(map_path: Path) -> FootprintData:
    """
    Parse GCC linker map file to extract memory section sizes.

    Example map file section:
        Memory Configuration
        Name             Origin             Length             Attributes
        FLASH            0x08000000         0x00080000         xr
        RAM              0x20000000         0x00020000         xrw

        Linker script and memory map
        ...
        .text           0x08000000     0x4a20
        .rodata         0x08004a20      0x350
        .data           0x20000000      0x100
        .bss            0x20000100      0x200
    """
    text = rodata = data = bss = 0

    with open(map_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # Pattern 1: Memory map section sizes (GCC ARM)
    # Example: .text           0x08000000     0x4a20
    patterns = [
        (r'\.text\s+0x[0-9a-f]+\s+0x([0-9a-f]+)', 'text'),
        (r'\.rodata\s+0x[0-9a-f]+\s+0x([0-9a-f]+)', 'rodata'),
        (r'\.data\s+0x[0-9a-f]+\s+0x([0-9a-f]+)', 'data'),
        (r'\.bss\s+0x[0-9a-f]+\s+0x([0-9a-f]+)', 'bss'),
    ]

    for pattern, section in patterns:
        match = re.search(pattern, content, re.IGNORECASE)
        if match:
            size = int(match.group(1), 16)
            if section == 'text':
                text = size
            elif section == 'rodata':
                rodata = size
            elif section == 'data':
                data = size
            elif section == 'bss':
                bss = size

    # Extract profile and target from path
    # Expected: build/footprint/cortex-m0plus/TINY.map
    parts = map_path.parts
    target = parts[-2] if len(parts) >= 2 else 'unknown'
    profile = map_path.stem.upper()

    return FootprintData(
        profile=profile,
        target=target,
        text=text,
        rodata=rodata,
        data=data,
        bss=bss
    )


def format_bytes(num_bytes: int) -> str:
    """Format bytes as human-readable string (KB, MB)."""
    if num_bytes < 1024:
        return f"{num_bytes} B"
    elif num_bytes < 1024 * 1024:
        return f"{num_bytes / 1024:.1f} KB"
    else:
        return f"{num_bytes / (1024 * 1024):.1f} MB"


def generate_markdown_table(footprints: List[FootprintData]) -> str:
    """Generate Markdown table from footprint data."""
    lines = []
    lines.append("## ROM/RAM Footprint")
    lines.append("")
    lines.append("| Profile | Target | ROM (text) | ROM (rodata) | Total ROM | RAM (data) | RAM (bss) | Total RAM |")
    lines.append("|---------|--------|------------|--------------|-----------|------------|-----------|-----------|")

    # Group by target, then by profile
    by_target: Dict[str, List[FootprintData]] = {}
    for fp in footprints:
        if fp.target not in by_target:
            by_target[fp.target] = []
        by_target[fp.target].append(fp)

    for target in sorted(by_target.keys()):
        for fp in sorted(by_target[target], key=lambda x: x.profile):
            lines.append(
                f"| {fp.profile:7s} | {fp.target:10s} | "
                f"{format_bytes(fp.text):>10s} | {format_bytes(fp.rodata):>12s} | "
                f"{format_bytes(fp.total_rom):>9s} | "
                f"{format_bytes(fp.data):>10s} | {format_bytes(fp.bss):>9s} | "
                f"{format_bytes(fp.total_ram):>9s} |"
            )

    return "\n".join(lines)


def generate_summary_table(footprints: List[FootprintData]) -> str:
    """Generate simplified summary table (ROM/RAM only)."""
    lines = []
    lines.append("## Resource Usage Summary")
    lines.append("")
    lines.append("| Profile | Cortex-M0+ ROM | Cortex-M0+ RAM | Cortex-M4F ROM | Cortex-M4F RAM |")
    lines.append("|---------|----------------|----------------|----------------|----------------|")

    # Group by profile
    by_profile: Dict[str, Dict[str, FootprintData]] = {}
    for fp in footprints:
        if fp.profile not in by_profile:
            by_profile[fp.profile] = {}
        by_profile[fp.profile][fp.target] = fp

    for profile in ['TINY', 'LEAN', 'FULL']:
        if profile not in by_profile:
            continue

        row = [f"| {profile:7s}"]
        
        for target in ['cortex-m0plus', 'cortex-m4f']:
            if target in by_profile[profile]:
                fp = by_profile[profile][target]
                row.append(f" {format_bytes(fp.total_rom):>14s}")
                row.append(f" {format_bytes(fp.total_ram):>14s}")
            else:
                row.append(f" {'N/A':>14s}")
                row.append(f" {'N/A':>14s}")

        row.append(" |")
        lines.append(" |".join(row).replace(" | ", " | "))

    return "\n".join(lines)


def compare_with_baseline(
    current: List[FootprintData],
    baseline_path: Path,
    threshold: float = 0.05
) -> Tuple[bool, str]:
    """
    Compare current footprint with baseline.

    Args:
        current: Current footprint measurements
        baseline_path: Path to baseline JSON file
        threshold: Acceptable increase (5% = 0.05)

    Returns:
        (passed, report_text) tuple
    """
    if not baseline_path.exists():
        return True, f"⚠️ Baseline file not found: {baseline_path}\n"

    with open(baseline_path, 'r') as f:
        baseline_data = json.load(f)

    lines = []
    lines.append("## Footprint Comparison vs Baseline")
    lines.append("")
    lines.append("| Profile | Target | Metric | Baseline | Current | Change | Status |")
    lines.append("|---------|--------|--------|----------|---------|--------|--------|")

    all_passed = True

    for fp in current:
        key = f"{fp.profile}_{fp.target}"
        if key not in baseline_data:
            lines.append(f"| {fp.profile} | {fp.target} | - | - | NEW | - | ⚠️ |")
            continue

        baseline = baseline_data[key]
        
        # Check ROM
        baseline_rom = baseline['total_rom']
        current_rom = fp.total_rom
        rom_change = (current_rom - baseline_rom) / baseline_rom if baseline_rom > 0 else 0
        rom_status = "✅" if rom_change <= threshold else "❌"
        if rom_change > threshold:
            all_passed = False

        lines.append(
            f"| {fp.profile} | {fp.target} | ROM | "
            f"{format_bytes(baseline_rom)} | {format_bytes(current_rom)} | "
            f"{rom_change:+.1%} | {rom_status} |"
        )

        # Check RAM
        baseline_ram = baseline['total_ram']
        current_ram = fp.total_ram
        ram_change = (current_ram - baseline_ram) / baseline_ram if baseline_ram > 0 else 0
        ram_status = "✅" if ram_change <= threshold else "❌"
        if ram_change > threshold:
            all_passed = False

        lines.append(
            f"| {fp.profile} | {fp.target} | RAM | "
            f"{format_bytes(baseline_ram)} | {format_bytes(current_ram)} | "
            f"{ram_change:+.1%} | {ram_status} |"
        )

    return all_passed, "\n".join(lines)


def save_as_baseline(footprints: List[FootprintData], output_path: Path) -> None:
    """Save current footprint as new baseline."""
    data = {}
    for fp in footprints:
        key = f"{fp.profile}_{fp.target}"
        data[key] = {
            'profile': fp.profile,
            'target': fp.target,
            'text': fp.text,
            'rodata': fp.rodata,
            'data': fp.data,
            'bss': fp.bss,
            'total_rom': fp.total_rom,
            'total_ram': fp.total_ram,
        }

    with open(output_path, 'w') as f:
        json.dump(data, f, indent=2)

    print(f"✅ Saved baseline to {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Measure ROM/RAM footprint from GCC map files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument(
        '--map',
        type=Path,
        help='Single map file to analyze'
    )
    parser.add_argument(
        '--target',
        type=str,
        choices=['cortex-m0plus', 'cortex-m4f', 'cortex-m33', 'riscv32'],
        help='Analyze all profiles for one target'
    )
    parser.add_argument(
        '--all',
        action='store_true',
        help='Analyze all targets and profiles'
    )
    parser.add_argument(
        '--baseline',
        type=Path,
        help='Path to baseline JSON file for comparison'
    )
    parser.add_argument(
        '--save-baseline',
        type=Path,
        help='Save current measurements as new baseline'
    )
    parser.add_argument(
        '--threshold',
        type=float,
        default=0.05,
        help='Acceptable increase threshold (default: 0.05 = 5%%)'
    )
    parser.add_argument(
        '--output',
        type=Path,
        help='Output Markdown report file'
    )

    args = parser.parse_args()

    # Collect map files to analyze
    map_files: List[Path] = []
    
    if args.map:
        if not args.map.exists():
            print(f"❌ Map file not found: {args.map}", file=sys.stderr)
            return 1
        map_files.append(args.map)
    
    elif args.target:
        # Find all profiles for this target
        target_dir = Path('build/footprint') / args.target
        if not target_dir.exists():
            print(f"❌ Target directory not found: {target_dir}", file=sys.stderr)
            return 1
        map_files = sorted(target_dir.glob('*.map'))
    
    elif args.all:
        # Find all map files
        footprint_dir = Path('build/footprint')
        if not footprint_dir.exists():
            print(f"❌ Footprint directory not found: {footprint_dir}", file=sys.stderr)
            print("Run CMake builds first to generate map files.", file=sys.stderr)
            return 1
        map_files = sorted(footprint_dir.glob('**/*.map'))
    
    else:
        parser.print_help()
        return 1

    if not map_files:
        print("❌ No map files found", file=sys.stderr)
        return 1

    # Parse all map files
    footprints: List[FootprintData] = []
    for map_file in map_files:
        try:
            fp = parse_gcc_map_file(map_file)
            footprints.append(fp)
            print(f"✅ Parsed {map_file.name}: ROM={format_bytes(fp.total_rom)}, RAM={format_bytes(fp.total_ram)}")
        except Exception as e:
            print(f"⚠️ Failed to parse {map_file}: {e}", file=sys.stderr)

    if not footprints:
        print("❌ No valid footprint data collected", file=sys.stderr)
        return 1

    # Generate report
    report_lines = []
    report_lines.append("# Modbus Library Footprint Report")
    report_lines.append("")
    report_lines.append(f"**Generated**: {Path.cwd()}")
    report_lines.append(f"**Map files analyzed**: {len(footprints)}")
    report_lines.append("")
    
    report_lines.append(generate_summary_table(footprints))
    report_lines.append("")
    report_lines.append(generate_markdown_table(footprints))
    report_lines.append("")

    # Compare with baseline if provided
    if args.baseline:
        passed, comparison = compare_with_baseline(footprints, args.baseline, args.threshold)
        report_lines.append(comparison)
        report_lines.append("")
        
        if not passed:
            print("\n❌ Footprint regression detected!", file=sys.stderr)
            print(comparison)
            if not args.output:
                return 1  # Fail CI

    # Save baseline if requested
    if args.save_baseline:
        save_as_baseline(footprints, args.save_baseline)

    # Write report
    report_text = "\n".join(report_lines)
    
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with open(args.output, 'w') as f:
            f.write(report_text)
        print(f"\n✅ Report written to {args.output}")
    else:
        print("\n" + report_text)

    return 0


if __name__ == '__main__':
    sys.exit(main())
