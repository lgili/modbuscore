#!/usr/bin/env python3
"""Run clang-format over all C/C headers in the repository."""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--clang-format", default="clang-format", help="Path to clang-format executable")
    args = parser.parse_args(argv)

    roots = [pathlib.Path("src"), pathlib.Path("include"), pathlib.Path("tests"), pathlib.Path("examples")] 
    files: list[str] = []
    for root in roots:
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.suffix in {".c", ".h"}:
                files.append(str(path))

    if not files:
        print("No C/C header files found.")
        return 0

    cmd = [args.clang_format, "-i", *files]
    print("Running:", " ".join(cmd))
    result = subprocess.run(cmd)
    return result.returncode


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
