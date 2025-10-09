#!/usr/bin/env python3
"""
Analyze worst-case stack usage from GCC .su files.

GCC can generate stack usage files with the -fstack-usage flag. This script:
1. Parses .su files to extract per-function stack usage
2. Builds call graphs to compute worst-case paths
3. Identifies critical paths (RX, TX, error handling)
4. Generates Markdown tables for documentation

Usage:
    # Analyze single .su file
    python3 scripts/ci/analyze_stack.py --su build/stack-usage/cortex-m0plus/TINY/modbus.c.su

    # Analyze all .su files for one target
    python3 scripts/ci/analyze_stack.py --target cortex-m0plus

    # Generate full report
    python3 scripts/ci/analyze_stack.py --all

    # Find worst-case path
    python3 scripts/ci/analyze_stack.py --all --critical-path mb_client_poll
"""

import argparse
import re
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


@dataclass
class StackUsage:
    """Stack usage for a single function."""
    function: str
    file: str
    line: int
    stack_bytes: int
    qualifier: str  # static, dynamic, bounded

    def __str__(self):
        return f"{self.function}: {self.stack_bytes} bytes ({self.qualifier})"


class CallGraph:
    """Simple call graph for worst-case analysis."""

    def __init__(self):
        self.functions: Dict[str, StackUsage] = {}
        self.calls: Dict[str, Set[str]] = defaultdict(set)  # caller -> callees

    def add_function(self, usage: StackUsage):
        """Add function with its stack usage."""
        self.functions[usage.function] = usage

    def add_call(self, caller: str, callee: str):
        """Record that caller calls callee."""
        self.calls[caller].add(callee)

    def worst_case_stack(self, entry_point: str, visited: Optional[Set[str]] = None) -> Tuple[int, List[str]]:
        """
        Compute worst-case stack for a call path starting at entry_point.

        Returns:
            (total_stack, path) tuple where path is list of function names
        """
        if visited is None:
            visited = set()

        # Prevent infinite recursion
        if entry_point in visited:
            return 0, []

        visited.add(entry_point)

        # Get this function's stack usage
        if entry_point not in self.functions:
            return 0, [entry_point + " (unknown)"]

        func_usage = self.functions[entry_point]
        local_stack = func_usage.stack_bytes

        # Find worst-case callee
        max_child_stack = 0
        max_child_path = []

        for callee in self.calls.get(entry_point, []):
            child_stack, child_path = self.worst_case_stack(callee, visited.copy())
            if child_stack > max_child_stack:
                max_child_stack = child_stack
                max_child_path = child_path

        total = local_stack + max_child_stack
        path = [entry_point] + max_child_path

        return total, path

    def find_entry_points(self) -> List[str]:
        """Find functions that are never called (potential entry points)."""
        all_callees = set()
        for callees in self.calls.values():
            all_callees.update(callees)

        entry_points = [
            func for func in self.functions.keys()
            if func not in all_callees
        ]

        return entry_points


def parse_su_file(su_path: Path) -> List[StackUsage]:
    """
    Parse GCC .su file.

    Example .su file format:
        modbus.c:123:5:mb_client_poll	256	static
        modbus.c:456:8:mb_decode_response	128	dynamic,bounded
    """
    usages = []

    with open(su_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            # Format: file:line:col:function<TAB>bytes<TAB>qualifier
            parts = line.split('\t')
            if len(parts) < 3:
                continue

            location, bytes_str, qualifier = parts[0], parts[1], parts[2]

            # Parse location (file:line:col:function)
            match = re.match(r'(.+?):(\d+):\d+:(.+)', location)
            if not match:
                continue

            file_name = match.group(1)
            line_num = int(match.group(2))
            func_name = match.group(3)

            # Parse stack bytes
            try:
                stack_bytes = int(bytes_str)
            except ValueError:
                continue

            usages.append(StackUsage(
                function=func_name,
                file=file_name,
                line=line_num,
                stack_bytes=stack_bytes,
                qualifier=qualifier
            ))

    return usages


def build_call_graph_from_source(source_dir: Path) -> CallGraph:
    """
    Build call graph by parsing C source files (simple heuristic).

    This is a SIMPLE heuristic that looks for function calls.
    For production, use a proper static analysis tool.
    """
    graph = CallGraph()

    # This is a placeholder - in production, use cscope, ctags, or clang AST
    # For now, we'll just track direct function calls with regex

    c_files = list(source_dir.glob('**/*.c'))

    for c_file in c_files:
        with open(c_file, 'r', errors='ignore') as f:
            content = f.read()

        # Find function definitions: return_type function_name(args) {
        func_defs = re.finditer(
            r'^\s*\w+[\s\*]+(\w+)\s*\([^)]*\)\s*\{',
            content,
            re.MULTILINE
        )

        for func_def in func_defs:
            func_name = func_def.group(1)

            # Find calls within this function (until next function def or end)
            func_start = func_def.end()
            
            # Simple heuristic: find function_name( patterns
            calls = re.finditer(r'\b(\w+)\s*\(', content[func_start:])
            
            for call in calls:
                callee = call.group(1)
                # Filter out keywords and common macros
                if callee not in ['if', 'while', 'for', 'switch', 'return', 'sizeof', 'printf']:
                    graph.add_call(func_name, callee)

    return graph


def generate_stack_report(
    usages: List[StackUsage],
    graph: Optional[CallGraph] = None,
    entry_points: Optional[List[str]] = None
) -> str:
    """Generate Markdown report from stack usage data."""
    lines = []
    lines.append("## Stack Usage Analysis")
    lines.append("")

    # Sort by stack usage (descending)
    usages_sorted = sorted(usages, key=lambda u: u.stack_bytes, reverse=True)

    # Top 10 stack consumers
    lines.append("### Top 10 Stack Consumers")
    lines.append("")
    lines.append("| Function | Stack (bytes) | Qualifier | File:Line |")
    lines.append("|----------|---------------|-----------|-----------|")

    for usage in usages_sorted[:10]:
        lines.append(
            f"| `{usage.function}` | {usage.stack_bytes} | {usage.qualifier} | "
            f"{usage.file}:{usage.line} |"
        )

    lines.append("")

    # Critical paths analysis
    if graph and entry_points:
        lines.append("### Critical Path Analysis")
        lines.append("")

        critical_functions = [
            'mb_client_poll',           # RX path
            'mb_client_send_request',   # TX path
            'mb_timeout_handler',       # Error path
            'mb_client_init',           # Init path
        ]

        for entry in critical_functions:
            if entry in graph.functions:
                total, path = graph.worst_case_stack(entry)
                lines.append(f"#### Path: `{entry}()`")
                lines.append("")
                lines.append(f"**Worst-case stack**: {total} bytes")
                lines.append("")
                lines.append("**Call chain**:")
                lines.append("```")
                for i, func in enumerate(path):
                    indent = "  " * i
                    if func in graph.functions:
                        usage = graph.functions[func]
                        lines.append(f"{indent}└─ {func}: {usage.stack_bytes} bytes")
                    else:
                        lines.append(f"{indent}└─ {func}")
                lines.append("```")
                lines.append("")

    # Summary statistics
    lines.append("### Summary")
    lines.append("")
    lines.append(f"- **Total functions analyzed**: {len(usages)}")
    lines.append(f"- **Max single function stack**: {usages_sorted[0].stack_bytes} bytes (`{usages_sorted[0].function}`)")
    lines.append(f"- **Average stack per function**: {sum(u.stack_bytes for u in usages) // len(usages)} bytes")
    
    # Count qualifiers
    static_count = sum(1 for u in usages if 'static' in u.qualifier)
    dynamic_count = sum(1 for u in usages if 'dynamic' in u.qualifier)
    lines.append(f"- **Static stack usage**: {static_count} functions")
    lines.append(f"- **Dynamic stack usage**: {dynamic_count} functions")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description='Analyze stack usage from GCC .su files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument(
        '--su',
        type=Path,
        help='Single .su file to analyze'
    )
    parser.add_argument(
        '--target',
        type=str,
        help='Analyze all .su files for one target'
    )
    parser.add_argument(
        '--all',
        action='store_true',
        help='Analyze all .su files'
    )
    parser.add_argument(
        '--source-dir',
        type=Path,
        default=Path('modbus/src'),
        help='Source directory for call graph analysis'
    )
    parser.add_argument(
        '--critical-path',
        type=str,
        help='Entry point function for worst-case analysis'
    )
    parser.add_argument(
        '--output',
        type=Path,
        help='Output Markdown report file'
    )

    args = parser.parse_args()

    # Collect .su files
    su_files: List[Path] = []

    if args.su:
        if not args.su.exists():
            print(f"❌ .su file not found: {args.su}", file=sys.stderr)
            return 1
        su_files.append(args.su)

    elif args.target:
        target_dir = Path('build/stack-usage') / args.target
        if not target_dir.exists():
            print(f"❌ Target directory not found: {target_dir}", file=sys.stderr)
            return 1
        su_files = sorted(target_dir.glob('**/*.su'))

    elif args.all:
        stack_dir = Path('build/stack-usage')
        if not stack_dir.exists():
            print(f"❌ Stack usage directory not found: {stack_dir}", file=sys.stderr)
            print("Build with -fstack-usage flag first.", file=sys.stderr)
            return 1
        su_files = sorted(stack_dir.glob('**/*.su'))

    else:
        parser.print_help()
        return 1

    if not su_files:
        print("❌ No .su files found", file=sys.stderr)
        return 1

    # Parse all .su files
    all_usages: List[StackUsage] = []
    for su_file in su_files:
        try:
            usages = parse_su_file(su_file)
            all_usages.extend(usages)
            print(f"✅ Parsed {su_file.name}: {len(usages)} functions")
        except Exception as e:
            print(f"⚠️ Failed to parse {su_file}: {e}", file=sys.stderr)

    if not all_usages:
        print("❌ No stack usage data collected", file=sys.stderr)
        return 1

    # Build call graph if requested
    graph = None
    if args.critical_path or args.source_dir.exists():
        print("🔍 Building call graph...")
        graph = CallGraph()
        
        # Add all functions to graph
        for usage in all_usages:
            graph.add_function(usage)

        # Build calls from source (simple heuristic)
        if args.source_dir.exists():
            source_graph = build_call_graph_from_source(args.source_dir)
            graph.calls = source_graph.calls

    # Analyze critical path if requested
    entry_points = None
    if args.critical_path and graph:
        total, path = graph.worst_case_stack(args.critical_path)
        print(f"\n📊 Worst-case stack for `{args.critical_path}()`: {total} bytes")
        print("\nCall chain:")
        for i, func in enumerate(path):
            indent = "  " * i
            if func in graph.functions:
                usage = graph.functions[func]
                print(f"{indent}└─ {func}: {usage.stack_bytes} bytes")
            else:
                print(f"{indent}└─ {func}")
        print()
        entry_points = [args.critical_path]
    elif graph:
        entry_points = graph.find_entry_points()

    # Generate report
    report = generate_stack_report(all_usages, graph, entry_points)

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with open(args.output, 'w') as f:
            f.write(report)
        print(f"✅ Report written to {args.output}")
    else:
        print("\n" + report)

    return 0


if __name__ == '__main__':
    sys.exit(main())
