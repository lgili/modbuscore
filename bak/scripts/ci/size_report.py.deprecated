#!/usr/bin/env python3
"""Lightweight footprint reporter for future CI metrics.

This script is intentionally conservative: it only depends on Python's
standard library and a GNU/LLVM `size` tool if one is available. The intent is
that Gate 30/35 work can plug richer logic on top of this skeleton without
reworking the CI wiring.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Dict, Iterable, List, Optional


@dataclass
class SizeEntry:
    """Captured size metrics for a single binary artifact."""

    path: str
    size_bytes: int
    text: Optional[int] = None
    data: Optional[int] = None
    bss: Optional[int] = None
    notes: Optional[str] = None


@dataclass
class FootprintReport:
    """Aggregate report saved to JSON for downstream jobs."""

    profile: str
    build_dir: str
    tool: Optional[str]
    entries: List[SizeEntry]

    def to_json(self) -> str:
        return json.dumps(asdict(self), indent=2, sort_keys=True)


def discover_binaries(build_dir: Path, explicit: Iterable[Path]) -> List[Path]:
    """Return the list of binaries to inspect.

    If explicit paths were provided, validate them. Otherwise attempt to find
    a small default set so the preset job works out-of-the-box.
    """

    build_dir = build_dir.resolve()
    if explicit:
        binaries: List[Path] = []
        for candidate in explicit:
            candidate = candidate if candidate.is_absolute() else build_dir / candidate
            if candidate.exists():
                binaries.append(candidate.resolve())
            else:
                raise FileNotFoundError(f"Binary not found: {candidate}")
        return binaries

    patterns = (
        "**/libmodbus.a",
        "**/libmodbus.so",
        "**/libmodbus.dylib",
        "**/modbus.lib",
        "**/modbus.dll",
    )
    discovered: Dict[str, Path] = {}
    for pattern in patterns:
        for hit in build_dir.glob(pattern):
            discovered[str(hit)] = hit.resolve()
    if not discovered:
        return []
    return sorted(discovered.values())


def locate_size_tool() -> Optional[str]:
    for candidate in ("llvm-size", "arm-none-eabi-size", "size"):
        tool = shutil.which(candidate)
        if tool:
            return tool
    return None


def read_segment_sizes(tool: str, binary: Path) -> Dict[str, int]:
    """Attempt to read text/data/bss sizes from the tool output.

    The parsing intentionally targets the simple BSD/GNU `size` layout and will
    fall back to an empty dict if the output is unexpected (for example when the
    target is a static archive listing every object file).
    """

    try:
        result = subprocess.run(
            [tool, str(binary)],
            check=True,
            text=True,
            capture_output=True,
        )
    except (subprocess.CalledProcessError, OSError):
        return {}

    lines = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    if not lines:
        return {}

    header, *data_lines = lines
    header_parts = header.split()
    if len(header_parts) < 4:
        return {}

    for line in reversed(data_lines):
        parts = line.split()
        if len(parts) < 4:
            continue
        # Many tools append the filename as the last column; tolerate both
        # "<values> <file>" and plain "<values>" rows.
        numeric = []
        for token in parts:
            if token.isdigit():
                numeric.append(int(token))
        if len(numeric) >= 3:
            text_size, data_size, bss_size = numeric[:3]
            return {"text": text_size, "data": data_size, "bss": bss_size}
    return {}


def build_report(args: argparse.Namespace) -> FootprintReport:
    build_dir = Path(args.build_dir).resolve()
    binaries = discover_binaries(build_dir, [Path(x) for x in args.binaries])

    tool = locate_size_tool()

    entries: List[SizeEntry] = []
    if not binaries and args.fail_missing:
        raise SystemExit(
            "No binaries were found. Pass --binary or disable --fail-missing."
        )

    for binary in binaries:
        info = read_segment_sizes(tool, binary) if tool else {}
        notes: Optional[str] = None
        if not info:
            notes = "segment sizes unavailable (tool missing or unsupported output)"
        entries.append(
            SizeEntry(
                path=str(binary.relative_to(build_dir) if binary.is_relative_to(build_dir) else binary),
                size_bytes=binary.stat().st_size,
                text=info.get("text"),
                data=info.get("data"),
                bss=info.get("bss"),
                notes=notes,
            )
        )

    return FootprintReport(
        profile=args.profile,
        build_dir=str(build_dir),
        tool=tool,
        entries=entries,
    )


def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Collect binary size information.")
    parser.add_argument(
        "--build-dir",
        default="build/host-footprint",
        help="Directory containing the compiled artifacts (default: build/host-footprint)",
    )
    parser.add_argument(
        "--profile",
        default="LEAN",
        help="Label used in the JSON output to indicate which profile was built.",
    )
    parser.add_argument(
        "--binary",
        dest="binaries",
        action="append",
        default=[],
        help="Relative or absolute path to a binary to analyse. Can be repeated.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Optional JSON file to write. Prints to stdout when omitted.",
    )
    parser.add_argument(
        "--fail-missing",
        action="store_true",
        help="Fail if no binaries were discovered instead of emitting an empty report.",
    )
    return parser.parse_args(argv)


def main(argv: Optional[List[str]] = None) -> int:
    args = parse_args(argv)
    report = build_report(args)
    payload = report.to_json()

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8")
    else:
        print(payload)
    return 0


if __name__ == "__main__":
    sys.exit(main())
