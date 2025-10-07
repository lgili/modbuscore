#!/usr/bin/env python3
"""Generate a size report for each supported profile.

The script configures and builds the library for the requested profiles and
collects ROM/RAM estimates from the resulting binary using the local toolchain's
`size` utility (llvm-size preferred).
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

DEFAULT_PROFILES = ("TINY", "LEAN", "FULL")
DEFAULT_TARGETS = ("host",)

TARGET_CONFIGS = {
    "host": {
        "build_subdir": "host",
        "toolchain": None,
        "generator": None,
        "preferred_size_tools": (),
        "cmake_args": [],
    },
    "stm32g0": {
        "build_subdir": "stm32g0",
        "toolchain": "cmake/toolchains/arm-none-eabi.cmake",
        "generator": "Ninja",
        "preferred_size_tools": ("arm-none-eabi-size",),
        "cmake_args": ["-DMODBUS_ENABLE_TESTS=OFF"],
    },
    "esp32c3": {
        "build_subdir": "esp32c3",
        "toolchain": "cmake/toolchains/riscv32-esp.cmake",
        "generator": "Ninja",
        "preferred_size_tools": ("riscv64-unknown-elf-size",),
        "cmake_args": ["-DMODBUS_ENABLE_TESTS=OFF"],
    },
}


def find_repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def run_command(cmd: List[str], cwd: Optional[Path] = None) -> None:
    result = subprocess.run(cmd, cwd=cwd, check=False, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"Command {' '.join(cmd)} failed with exit code {result.returncode}"
        )


def configure_build(
    source_dir: Path,
    build_dir: Path,
    profile: str,
    generator: Optional[str],
    extra_args: Optional[List[str]] = None,
) -> None:
    build_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        "cmake",
        "-S",
        str(source_dir),
        "-B",
        str(build_dir),
        f"-DMODBUS_PROFILE={profile}",
        "-DMODBUS_ENABLE_TESTS=OFF",
        "-DMODBUS_BUILD_DOCS=OFF",
        "-DMODBUS_BUILD_EXAMPLES=OFF",
        "-DMODBUS_BUILD_FUZZERS=OFF",
        "-DCMAKE_BUILD_TYPE=MinSizeRel",
    ]
    if generator:
        cmd.extend(["-G", generator])
    if extra_args:
        cmd.extend(extra_args)
    run_command(cmd)


def build_target(build_dir: Path, target: str) -> None:
    run_command(["cmake", "--build", str(build_dir), "--target", target])


def find_artifact(build_dir: Path) -> Path:
    candidates = list(build_dir.glob("**/libmodbus.a"))
    if not candidates:
        candidates = list(build_dir.glob("**/libmodbus.so"))
    if not candidates:
        candidates = list(build_dir.glob("**/libmodbus.dylib"))
    if not candidates:
        raise FileNotFoundError(f"Unable to locate a built modbus artifact under {build_dir}")
    # Prefer the shortest path (typically the primary library output)
    return min(candidates, key=lambda p: len(p.parts))


def pick_size_tool(preferred: Iterable[str] | None = None) -> str:
    if preferred:
        for tool in preferred:
            path = shutil.which(tool)
            if path:
                return path
    for tool in ("llvm-size", "size"):
        path = shutil.which(tool)
        if path:
            return path
    raise FileNotFoundError("Neither llvm-size nor size was found in PATH.")


def collect_size(tool: str, artifact: Path) -> Dict[str, int]:
    cmd = [tool, "--format=berkeley", str(artifact)] if "llvm-size" in tool else [tool, str(artifact)]
    completed = subprocess.run(cmd, check=False, capture_output=True, text=True)
    if completed.returncode != 0:
        raise RuntimeError(f"{' '.join(cmd)} failed: {completed.stderr.strip()}")

    lines = [line for line in completed.stdout.splitlines() if line.strip()]
    if not lines:
        raise RuntimeError("size output was empty")

    # Berkeley format ends with: text data bss dec hex filename
    fields = lines[-1].split()
    if len(fields) < 6:
        raise RuntimeError(f"Unexpected size output: {lines[-1]}")

    text, data, bss = (int(fields[i]) for i in range(3))
    return {
        "rom_bytes": text + data,
        "ram_bytes": bss + data,
        "text": text,
        "data": data,
        "bss": bss,
    }


def collect_object_totals(tool: str, build_dir: Path) -> Tuple[int, int, int]:
    objects = sorted(build_dir.glob("modbus/CMakeFiles/modbus.dir/**/*.o"))
    if not objects:
        return (0, 0, 0)

    total_text = 0
    total_data = 0
    total_bss = 0

    for obj in objects:
        cmd = [tool, "--format=berkeley", str(obj)] if "llvm-size" in tool else [tool, str(obj)]
        result = subprocess.run(cmd, check=False, capture_output=True, text=True)
        if result.returncode != 0 or not result.stdout:
            raise RuntimeError(f"Failed to inspect object size for {obj}: {result.stderr.strip()}")
        # Drop the header row and pick the last line (payload)
        lines = [ln for ln in result.stdout.splitlines() if ln.strip()]
        line = lines[-1]
        parts = line.split()
        if len(parts) < 5:
            raise RuntimeError(f"Unexpected size output for {obj}: {line}")
        text = int(parts[0])
        data = int(parts[1])
        if "llvm-size" in tool:
            bss = int(parts[3])
        else:
            bss = int(parts[2])
        total_text += text
        total_data += data
        total_bss += bss

    return (total_text, total_data, total_bss)


def format_markdown_table(entries: List[Dict[str, object]]) -> str:
    lines = [
        "| Target | Profile | ROM (archive) | ROM (objects) | RAM (objects) |",
        "|--------|---------|---------------|---------------|---------------|",
    ]
    for entry in entries:
        lines.append(
            f"| {entry['target']} | {entry['profile']} | {entry['archive_rom_bytes']} | {entry['object_rom_bytes']} | {entry['object_ram_bytes']} |"
        )
    return "\n".join(lines)


def update_readme_table(readme_path: Path, table_md: str) -> None:
    content = readme_path.read_text()
    start_marker = "<!-- footprint:start -->"
    end_marker = "<!-- footprint:end -->"
    start_idx = content.find(start_marker)
    end_idx = content.find(end_marker)
    if start_idx == -1 or end_idx == -1:
        raise RuntimeError("Footprint markers not found in README")

    start_idx += len(start_marker)
    new_content = (
        content[:start_idx]
        + "\n"
        + table_md
        + "\n"
        + content[end_idx:]
    )
    readme_path.write_text(new_content)


def main(argv: Optional[Iterable[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Report ROM/RAM usage per Modbus profile.")
    parser.add_argument("--profiles", nargs="*", default=list(DEFAULT_PROFILES), help="Profiles to evaluate (default: TINY LEAN FULL)")
    parser.add_argument("--build-root", default=str(find_repo_root() / "build" / "footprint"), help="Directory used for build trees")
    parser.add_argument("--generator", default=None, help="Optional fallback CMake generator when target config does not specify one (e.g., Ninja)")
    parser.add_argument("--targets", nargs="*", default=list(DEFAULT_TARGETS), help=f"Targets to evaluate (choices: {', '.join(TARGET_CONFIGS)})")
    parser.add_argument("--output", default=None, help="Optional path to write JSON results")
    parser.add_argument("--update-readme", default=None, help="Path to README.md to update table")
    args = parser.parse_args(list(argv) if argv is not None else None)

    source_dir = find_repo_root()
    build_root = Path(args.build_root)

    raw_targets = args.targets if args.targets else list(DEFAULT_TARGETS)
    targets: List[str] = []
    seen_targets: set[str] = set()
    for tgt in raw_targets:
        key = tgt.lower()
        if key not in TARGET_CONFIGS:
            raise ValueError(f"Unknown target '{tgt}'. Valid options: {', '.join(TARGET_CONFIGS)}")
        if key in seen_targets:
            continue
        targets.append(key)
        seen_targets.add(key)

    results: List[Dict[str, object]] = []
    for target in targets:
        target_key = target
        config = TARGET_CONFIGS[target_key]
        target_build_root = build_root / config.get("build_subdir", target_key)

        preferred_tools = config.get("preferred_size_tools", ())
        size_tool = pick_size_tool(preferred_tools)

        generator = config.get("generator") or args.generator

        extra_args: List[str] = list(config.get("cmake_args", []))
        toolchain = config.get("toolchain")
        if toolchain:
            extra_args.append(f"-DCMAKE_TOOLCHAIN_FILE={source_dir / toolchain}")

        for profile in args.profiles:
            profile_name = profile.upper()
            build_dir = target_build_root / profile_name.lower()
            configure_build(source_dir, build_dir, profile_name, generator, extra_args)
            build_target(build_dir, "modbus")
            artifact = find_artifact(build_dir)
            archive_footprint = collect_size(size_tool, artifact)
            text, data, bss = collect_object_totals(size_tool, build_dir)
            artifact_path = artifact.resolve()
            try:
                artifact_display = str(artifact_path.relative_to(source_dir))
            except ValueError:
                artifact_display = str(artifact)
            footprint = {
                "target": target_key,
                "profile": profile_name,
                "artifact": artifact_display,
                "archive_rom_bytes": archive_footprint["rom_bytes"],
                "archive_ram_bytes": archive_footprint["ram_bytes"],
                "object_rom_bytes": text + data,
                "object_ram_bytes": data + bss,
                "text": text,
                "data": data,
                "bss": bss,
            }
            results.append(footprint)

    target_order = {t: idx for idx, t in enumerate(targets)}
    profile_order = {p.upper(): idx for idx, p in enumerate(args.profiles)}
    results.sort(key=lambda e: (target_order.get(str(e["target"]).lower(), 0), profile_order.get(str(e["profile"]).upper(), 0)))

    # Pretty table for logs
    header = (
        f"{'Target':<10} {'Profile':<8} {'ROM (archive)':>14} "
        f"{'ROM (objects)':>14} {'RAM (objects)':>14} {'Artifact'}"
    )
    print(header)
    print("-" * len(header))
    for entry in results:
        print(
            f"{entry['target']:<10} {entry['profile']:<8} {entry['archive_rom_bytes']:>14} "
            f"{entry['object_rom_bytes']:>14} {entry['object_ram_bytes']:>14} {entry['artifact']}"
        )

    if args.output:
        output_path = Path(args.output)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(results, indent=2))

    if args.update_readme:
        table_md = format_markdown_table(results)
        update_readme_table(Path(args.update_readme), table_md)

    return 0


if __name__ == "__main__":
    sys.exit(main())
