#!/usr/bin/env python3
"""Sanity exercises for the Modbus CLI (used by ctest)."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def run_cli(cli_path: Path, args: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess:
    tools_dir = cli_path.parent.parent / "tools"
    env = os.environ.copy()
    existing_py_path = env.get("PYTHONPATH", "")
    env["PYTHONPATH"] = str(tools_dir) + (os.pathsep + existing_py_path if existing_py_path else "")

    cmd = [sys.executable, "-m", "modbus_cli.cli", *args]
    result = subprocess.run(
        cmd,
        cwd=cwd,
        capture_output=True,
        text=True,
        env=env,
    )
    if result.returncode != 0:
        print(f"Command failed: {' '.join(cmd)}", file=sys.stderr)
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
    return result


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    cli_script = repo_root / "scripts" / "modbus"
    if not cli_script.exists():
        print(f"CLI wrapper not found at {cli_script}", file=sys.stderr)
        return 1

    tmp_base = repo_root / "build" / "cli_sanity"
    tmp_base.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="modbus_cli_", dir=tmp_base) as tmpdir:
        tmp_path = Path(tmpdir)

        app_dir = tmp_path / "demo_app"
        result = run_cli(cli_script, ["new", "app", "demo_app", "--transport", "tcp", "--unit", "5", "--out", str(app_dir)])
        if result.returncode != 0:
            return 1
        if not (app_dir / "src" / "main.c").exists():
            print("Generated app missing src/main.c", file=sys.stderr)
            return 1

        profile_dir = tmp_path / "profile_app"
        result = run_cli(cli_script, ["new", "profile", "posix-tcp-client", "--out", str(profile_dir)])
        if result.returncode != 0:
            return 1
        readme_content = (profile_dir / "README.md").read_text(encoding="utf-8")
        if "Perfil: posix-tcp-client" not in readme_content:
            print("Profile README missing expected section", file=sys.stderr)
            return 1

    # Doctor should succeed (ignoring output)
    doctor = run_cli(cli_script, ["doctor"])
    if doctor.returncode != 0:
        print("modbus doctor reported errors", file=sys.stderr)
        print(doctor.stdout, file=sys.stderr)
        print(doctor.stderr, file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
