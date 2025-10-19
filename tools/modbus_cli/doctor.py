"""Environment diagnostics for the Modbus CLI."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from dataclasses import asdict, dataclass
from typing import Dict, List, Optional


@dataclass
class CheckResult:
    name: str
    ok: bool
    message: str
    detail: Optional[str] = None
    optional: bool = False


def run_doctor(json_output: bool = False) -> int:
    checks: List[CheckResult] = []

    checks.append(check_python())
    checks.append(check_command("cmake", ["cmake", "--version"], "CMake >= 3.15"))
    checks.append(check_command("c compiler", _detect_c_compiler(), "C compiler (gcc/clang/msvc)"))
    checks.extend(check_platform_specific())

    has_errors = any(not c.ok and not c.optional for c in checks)

    for check in checks:
        if check.ok:
            status = "OK"
        elif check.optional:
            status = "WARN"
        else:
            status = "FAIL"
        print(f"[{status}] {check.name} - {check.message}")
        if check.detail:
            print(f"       {check.detail}")

    if json_output:
        payload = [asdict(c) for c in checks]
        print(json.dumps(payload, indent=2), file=sys.stderr)

    return 0 if not has_errors else 1


def check_python() -> CheckResult:
    version = sys.version.split()[0]
    major, minor, *_ = (int(part) for part in version.split("."))
    if major >= 3 and minor >= 8:
        return CheckResult("python", True, f"Python {version} (OK)")
    return CheckResult("python", False, f"Python {version} (expected >= 3.8)")


def check_command(name: str, command: Optional[List[str]], hint: str) -> CheckResult:
    if not command:
        return CheckResult(name, False, f"{hint} not found in PATH")
    try:
        completed = subprocess.run(command, capture_output=True, text=True, check=False)
    except FileNotFoundError:
        return CheckResult(name, False, f"{hint} not found in PATH")

    if completed.returncode != 0:
        return CheckResult(
            name,
            False,
            f"Failed to execute {' '.join(command)}",
            detail=completed.stderr.strip(),
        )

    return CheckResult(
        name,
        True,
        completed.stdout.splitlines()[0] if completed.stdout else f"{hint} found",
    )


def _detect_c_compiler() -> Optional[List[str]]:
    for candidate in ("clang", "gcc", "cc"):
        location = shutil.which(candidate)
        if location:
            return [candidate, "--version"]
    # Windows Visual Studio
    cl = shutil.which("cl")
    if cl:
        return ["cl"]
    return None


def check_platform_specific() -> List[CheckResult]:
    results: List[CheckResult] = []
    if sys.platform.startswith("win"):
        # Winsock TCP example requires Visual Studio Build Tools (cl)
        if shutil.which("cl"):
            results.append(CheckResult("msvc", True, "cl (Visual Studio) available"))
        else:
            results.append(
                CheckResult(
                    "msvc",
                    False,
                    "cl (Visual Studio) not found",
                    detail="Optional, install Visual Studio Build Tools or use MinGW/clang",
                    optional=True,
                )
            )
    else:
        # POSIX: check for socat (useful for RTU loopback testing)
        if shutil.which("socat"):
            results.append(CheckResult("socat", True, "socat available (useful for RTU loopback)"))
        else:
            results.append(
                CheckResult(
                    "socat",
                    False,
                    "socat not found",
                    detail="Optional, but recommended for RTU loopback testing",
                    optional=True,
                )
            )
    return results

