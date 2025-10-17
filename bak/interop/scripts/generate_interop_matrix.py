#!/usr/bin/env python3
"""
Gate 26: Interoperability Matrix Generator
===========================================

Generates a compatibility matrix showing which implementations can
successfully communicate with each other.

Output formats:
- HTML (visual matrix)
- JSON (machine-readable)
- Markdown (documentation)
"""

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, List
from collections import defaultdict


class InteropMatrixGenerator:
    """Generates interoperability compatibility matrix"""
    
    def __init__(self, results_file: Path):
        self.results_file = results_file
        self.results_data = None
        self.matrix = defaultdict(lambda: defaultdict(dict))
    
    def load_results(self) -> bool:
        """Load test results from JSON"""
        if not self.results_file.exists():
            print(f"Error: Results file not found: {self.results_file}", file=sys.stderr)
            return False
        
        try:
            with open(self.results_file) as f:
                self.results_data = json.load(f)
            return True
        except Exception as e:
            print(f"Error loading results: {e}", file=sys.stderr)
            return False
    
    def build_matrix(self):
        """Build compatibility matrix from results"""
        for result in self.results_data.get('results', []):
            server = result['server']
            client = result['client']
            success = result['success']
            duration = result['duration_ms']
            scenario = result['scenario']
            
            # Extract function code from scenario name
            if scenario.startswith('fc'):
                fc = scenario[2:4]
            else:
                fc = 'unknown'
            
            if fc not in self.matrix[server][client]:
                self.matrix[server][client][fc] = []
            
            self.matrix[server][client][fc].append({
                'success': success,
                'duration_ms': duration,
                'scenario': scenario
            })
    
    def generate_markdown(self) -> str:
        """Generate Markdown format matrix"""
        md = ["# Modbus Interoperability Matrix (Gate 26)", ""]
        md.append(f"**Generated:** {self.results_data.get('timestamp', 'N/A')}")
        md.append(f"**Total Tests:** {self.results_data.get('total_tests', 0)}")
        md.append(f"**Passed:** {self.results_data.get('passed', 0)}")
        md.append(f"**Failed:** {self.results_data.get('failed', 0)}")
        md.append("")
        
        # Overall compatibility matrix
        md.append("## Compatibility Matrix")
        md.append("")
        md.append("✅ = All tests passed | ⚠️ = Some failures | ❌ = All failed | - = Not tested")
        md.append("")
        
        implementations = sorted(set(
            [r['server'] for r in self.results_data.get('results', [])] +
            [r['client'] for r in self.results_data.get('results', [])]
        ))
        
        # Build table header
        header = "| Server \\ Client |"
        separator = "|------------------|"
        for impl in implementations:
            header += f" {impl} |"
            separator += "---------|"
        md.append(header)
        md.append(separator)
        
        # Build table rows
        for server in implementations:
            row = f"| **{server}** |"
            for client in implementations:
                if server == client:
                    row += " - |"
                    continue
                
                if client in self.matrix[server]:
                    results = []
                    for fc_results in self.matrix[server][client].values():
                        results.extend(fc_results)
                    
                    if results:
                        passed = sum(1 for r in results if r['success'])
                        total = len(results)
                        
                        if passed == total:
                            row += " ✅ |"
                        elif passed > 0:
                            row += f" ⚠️ {passed}/{total} |"
                        else:
                            row += " ❌ |"
                    else:
                        row += " - |"
                else:
                    row += " - |"
            md.append(row)
        
        md.append("")
        
        # Function code breakdown
        md.append("## Function Code Support")
        md.append("")
        
        fc_names = {
            '03': 'Read Holding Registers',
            '04': 'Read Input Registers',
            '06': 'Write Single Register',
            '16': 'Write Multiple Registers',
        }
        
        for fc, fc_name in fc_names.items():
            md.append(f"### FC {fc}: {fc_name}")
            md.append("")
            
            # Build FC-specific table
            header = "| Server \\ Client |"
            separator = "|------------------|"
            for impl in implementations:
                header += f" {impl} |"
                separator += "---------|"
            md.append(header)
            md.append(separator)
            
            for server in implementations:
                row = f"| **{server}** |"
                for client in implementations:
                    if server == client:
                        row += " - |"
                        continue
                    
                    if client in self.matrix[server] and fc in self.matrix[server][client]:
                        results = self.matrix[server][client][fc]
                        passed = sum(1 for r in results if r['success'])
                        total = len(results)
                        
                        if passed == total:
                            row += " ✅ |"
                        elif passed > 0:
                            row += f" ⚠️ |"
                        else:
                            row += " ❌ |"
                    else:
                        row += " - |"
                md.append(row)
            
            md.append("")
        
        return "\n".join(md)
    
    def generate_html(self) -> str:
        """Generate HTML format matrix"""
        html = [
            "<!DOCTYPE html>",
            "<html>",
            "<head>",
            "    <meta charset='UTF-8'>",
            "    <title>Modbus Interoperability Matrix - Gate 26</title>",
            "    <style>",
            "        body { font-family: Arial, sans-serif; margin: 20px; }",
            "        h1 { color: #333; }",
            "        .summary { background: #f5f5f5; padding: 15px; border-radius: 5px; margin: 20px 0; }",
            "        table { border-collapse: collapse; width: 100%; margin: 20px 0; }",
            "        th, td { border: 1px solid #ddd; padding: 12px; text-align: center; }",
            "        th { background-color: #4CAF50; color: white; }",
            "        tr:nth-child(even) { background-color: #f2f2f2; }",
            "        .pass { color: #4CAF50; font-weight: bold; }",
            "        .fail { color: #f44336; font-weight: bold; }",
            "        .partial { color: #ff9800; font-weight: bold; }",
            "        .not-tested { color: #999; }",
            "    </style>",
            "</head>",
            "<body>",
            "    <h1>Modbus Interoperability Matrix (Gate 26)</h1>",
            "    <div class='summary'>",
            f"        <p><strong>Generated:</strong> {self.results_data.get('timestamp', 'N/A')}</p>",
            f"        <p><strong>Total Tests:</strong> {self.results_data.get('total_tests', 0)}</p>",
            f"        <p><strong>Passed:</strong> <span class='pass'>{self.results_data.get('passed', 0)}</span></p>",
            f"        <p><strong>Failed:</strong> <span class='fail'>{self.results_data.get('failed', 0)}</span></p>",
            "    </div>",
            "    <h2>Compatibility Matrix</h2>",
            "    <table>",
        ]
        
        implementations = sorted(set(
            [r['server'] for r in self.results_data.get('results', [])] +
            [r['client'] for r in self.results_data.get('results', [])]
        ))
        
        # Header row
        html.append("        <tr><th>Server \\ Client</th>")
        for impl in implementations:
            html.append(f"<th>{impl}</th>")
        html.append("</tr>")
        
        # Data rows
        for server in implementations:
            html.append(f"        <tr><th>{server}</th>")
            for client in implementations:
                if server == client:
                    html.append("<td class='not-tested'>-</td>")
                    continue
                
                if client in self.matrix[server]:
                    results = []
                    for fc_results in self.matrix[server][client].values():
                        results.extend(fc_results)
                    
                    if results:
                        passed = sum(1 for r in results if r['success'])
                        total = len(results)
                        
                        if passed == total:
                            html.append("<td class='pass'>✅</td>")
                        elif passed > 0:
                            html.append(f"<td class='partial'>⚠️ {passed}/{total}</td>")
                        else:
                            html.append("<td class='fail'>❌</td>")
                    else:
                        html.append("<td class='not-tested'>-</td>")
                else:
                    html.append("<td class='not-tested'>-</td>")
            html.append("</tr>")
        
        html.extend([
            "    </table>",
            "</body>",
            "</html>"
        ])
        
        return "\n".join(html)
    
    def generate_json(self) -> str:
        """Generate JSON format matrix"""
        matrix_dict = {}
        for server, clients in self.matrix.items():
            matrix_dict[server] = {}
            for client, fcs in clients.items():
                matrix_dict[server][client] = dict(fcs)
        
        output = {
            "metadata": {
                "timestamp": self.results_data.get('timestamp'),
                "total_tests": self.results_data.get('total_tests'),
                "passed": self.results_data.get('passed'),
                "failed": self.results_data.get('failed')
            },
            "matrix": matrix_dict
        }
        
        return json.dumps(output, indent=2)


def main():
    parser = argparse.ArgumentParser(description="Generate interop matrix (Gate 26)")
    parser.add_argument("--results", type=Path, default=Path("/results/interop_results.json"),
                       help="Path to interop results JSON")
    parser.add_argument("--format", choices=['markdown', 'html', 'json'], default='markdown',
                       help="Output format")
    parser.add_argument("--output", type=Path, help="Output file (default: stdout)")
    
    args = parser.parse_args()
    
    generator = InteropMatrixGenerator(args.results)
    
    if not generator.load_results():
        return 1
    
    generator.build_matrix()
    
    # Generate output
    if args.format == 'markdown':
        output = generator.generate_markdown()
    elif args.format == 'html':
        output = generator.generate_html()
    elif args.format == 'json':
        output = generator.generate_json()
    else:
        print(f"Unknown format: {args.format}", file=sys.stderr)
        return 1
    
    # Write output
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with open(args.output, 'w') as f:
            f.write(output)
        print(f"Matrix written to {args.output}")
    else:
        print(output)
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
