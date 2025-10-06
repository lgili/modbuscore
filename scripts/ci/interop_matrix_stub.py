#!/usr/bin/env python3
"""Placeholder for future interoperability matrix automation.

This keeps Gate 26 (+ Gate 36 later on) lightweight by providing a canonical
entry point that is already wired into CI. Once real interoperability tests are
ready, replace the stub logic below with the actual orchestration (spawning
containers, collecting pcaps, etc.).
"""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import List, Optional


def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate an interop matrix stub report.")
    parser.add_argument(
        "--output",
        type=Path,
        help="Optional JSON file to emit. If omitted, the payload is printed to stdout.",
    )
    parser.add_argument(
        "--notes",
        default="Awaiting real interoperability runners",
        help="Short human-readable status message.",
    )
    return parser.parse_args(argv)


def build_payload(notes: str) -> str:
    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "status": "stub",
        "notes": notes,
        "matrix": [],
    }
    return json.dumps(payload, indent=2, sort_keys=True)


def main(argv: Optional[List[str]] = None) -> int:
    args = parse_args(argv)
    payload = build_payload(args.notes)

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8")
    else:
        print(payload)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
