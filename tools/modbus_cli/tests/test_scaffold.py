#!/usr/bin/env python3
"""End-to-end smoke test for the `modbus` CLI scaffolding."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def log(msg: str) -> None:
    print(f"[scaffold] {msg}")


def run(cmd: list[str], **kwargs) -> None:
    log("$ " + " ".join(cmd))
    subprocess.run(cmd, check=True, **kwargs)


def generate_and_build(repo_root: Path, build_root: Path, python_exe: str) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        install_dir = tmp_path / "install"
        project_dir = tmp_path / "autoheal_demo"

        env = os.environ.copy()
        env_paths = [repo_root.as_posix()]
        if env.get("PYTHONPATH"):
            env_paths.append(env["PYTHONPATH"])
        env["PYTHONPATH"] = os.pathsep.join(env_paths)

        run(
            [
                python_exe,
                "-m",
                "tools.modbus_cli.cli",
                "new",
                "app",
                "autoheal_demo",
                "--out",
                str(project_dir),
                "--auto-heal",
            ],
            env=env,
        )

        install_dir.mkdir()
        install_cmd = ["cmake", "--install", str(build_root), "--prefix", str(install_dir)]
        if os.name == "nt":
            install_cmd.extend(["--config", "Debug"])
        run(install_cmd)

        build_dir = project_dir / "build"
        run(
            [
                "cmake",
                "-S",
                str(project_dir),
                "-B",
                str(build_dir),
                f"-DCMAKE_PREFIX_PATH={install_dir.as_posix()}",
            ]
        )
        run(["cmake", "--build", str(build_dir)])


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="CLI scaffold smoke test")
    parser.add_argument("--repo-root", required=True, type=Path)
    parser.add_argument("--build-root", required=True, type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    build_root = args.build_root.resolve()

    log(f"repo={repo_root}")
    log(f"build={build_root}")
    log(f"python={sys.executable}")

    generate_and_build(repo_root, build_root, sys.executable)
    log("scaffold ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
