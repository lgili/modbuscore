#!/usr/bin/env python3
"""Run clang-tidy over source files using compile_commands.json."""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--clang-tidy", default="clang-tidy", help="Path to clang-tidy executable")
    parser.add_argument("--build-dir", default="build", help="CMake build directory containing compile_commands.json")
    args = parser.parse_args(argv)

    files = [str(path) for path in pathlib.Path("src").rglob("*.c")]
    if not files:
        print("No C sources found under ./src")
        return 0

    overall_status = 0
    for path in files:
        cmd = [args.clang_tidy, path, "-p", args.build_dir]
        print("Running:", " ".join(cmd))
        result = subprocess.run(cmd)
        if result.returncode != 0:
            overall_status = result.returncode
    if overall_status != 0:
        print("clang-tidy reported issues (see output above)")
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
