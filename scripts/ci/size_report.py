#!/usr/bin/env python3
"""
⚠️ DEPRECATED: This script skeleton has been replaced by report_footprint.py

This file is kept for reference only. The functionality has been integrated into
report_footprint.py, which provides:
- Full build automation
- Map file parsing (detailed ROM breakdown)
- Baseline comparison & regression detection
- README.md integration
- Multi-target support (host, stm32g0, esp32c3)

Use report_footprint.py instead:
    python3 scripts/ci/report_footprint.py --help

Or:
    make footprint        # Build and measure
    make footprint-check  # Check against baseline

This file will be removed in the next major release.
"""

import sys

def main():
    print("⚠️ ERROR: size_report.py has been deprecated.")
    print("")
    print("Please use report_footprint.py instead:")
    print("  python3 scripts/ci/report_footprint.py --help")
    print("")
    print("Or use Makefile targets:")
    print("  make footprint        # Build and measure")
    print("  make footprint-check  # Check against baseline")
    return 1

if __name__ == "__main__":
    sys.exit(main())
