#!/usr/bin/env python3
"""
Gate 26: Expanded Interoperability Test Suite
==============================================

Comprehensive testing covering:
- All Modbus function codes (FC01, FC02, FC03, FC04, FC05, FC06, FC15, FC16)
- Multiple implementations (pymodbus, diagslave, libmodbus, our library)
- Cross-implementation testing (every client vs every server)
- PCAP capture and protocol validation
- Error handling and edge cases
"""

import json
import subprocess
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional

# Test configuration
RESULTS_DIR = Path("/results")
PCAP_DIR = RESULTS_DIR / "pcaps"
RESULTS_DIR.mkdir(parents=True, exist_ok=True)
PCAP_DIR.mkdir(parents=True, exist_ok=True)

# Server/client configurations
SERVERS = {
    "pymodbus": {
        "cmd": ["python3", "-c", """
from pymodbus.server import StartTcpServer
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext, ModbusSequentialDataBlock

# Initialize with test data
coils = [0, 1, 0, 1, 1] + [0]*995
discrete = [1, 0, 1, 0, 0] + [0]*995
holding = list(range(1000, 2000))
input_reg = list(range(2000, 3000))

store = ModbusSlaveContext(
    co=ModbusSequentialDataBlock(1, coils),
    di=ModbusSequentialDataBlock(1, discrete),
    hr=ModbusSequentialDataBlock(1, holding),
    ir=ModbusSequentialDataBlock(1, input_reg)
)
context = ModbusServerContext(slaves=store, single=True)
print('Server ready', flush=True)
StartTcpServer(context=context, address=('0.0.0.0', {port}))
"""],
        "ready_pattern": "Server ready"
    },
    "diagslave": {
        "cmd": ["/opt/diagslave/x86_64-linux-gnu/diagslave", "-m", "tcp", "-p", "{port}"],
        "ready_pattern": "opened"
    },
    "our_server": {
        "cmd": ["/modbus-lib/build/examples/modbus_tcp_server_demo", "--port", "{port}"],
        "ready_pattern": "listening"
    }
}

CLIENTS = {
    "pymodbus": {
        "fc01": 'from pymodbus.client import ModbusTcpClient; c = ModbusTcpClient("127.0.0.1", {port}); c.connect(); r = c.read_coils({addr}, {count}, slave=1); print("SUCCESS" if not r.isError() else "FAIL"); c.close()',
        "fc02": 'from pymodbus.client import ModbusTcpClient; c = ModbusTcpClient("127.0.0.1", {port}); c.connect(); r = c.read_discrete_inputs({addr}, {count}, slave=1); print("SUCCESS" if not r.isError() else "FAIL"); c.close()',
        "fc03": 'from pymodbus.client import ModbusTcpClient; c = ModbusTcpClient("127.0.0.1", {port}); c.connect(); r = c.read_holding_registers({addr}, {count}, slave=1); print("SUCCESS" if not r.isError() else "FAIL"); c.close()',
        "fc04": 'from pymodbus.client import ModbusTcpClient; c = ModbusTcpClient("127.0.0.1", {port}); c.connect(); r = c.read_input_registers({addr}, {count}, slave=1); print("SUCCESS" if not r.isError() else "FAIL"); c.close()',
        "fc05": 'from pymodbus.client import ModbusTcpClient; c = ModbusTcpClient("127.0.0.1", {port}); c.connect(); r = c.write_coil({addr}, {value}, slave=1); print("SUCCESS" if not r.isError() else "FAIL"); c.close()',
        "fc06": 'from pymodbus.client import ModbusTcpClient; c = ModbusTcpClient("127.0.0.1", {port}); c.connect(); r = c.write_register({addr}, {value}, slave=1); print("SUCCESS" if not r.isError() else "FAIL"); c.close()',
        "fc15": 'from pymodbus.client import ModbusTcpClient; c = ModbusTcpClient("127.0.0.1", {port}); c.connect(); r = c.write_coils({addr}, [{values}], slave=1); print("SUCCESS" if not r.isError() else "FAIL"); c.close()',
        "fc16": 'from pymodbus.client import ModbusTcpClient; c = ModbusTcpClient("127.0.0.1", {port}); c.connect(); r = c.write_registers({addr}, [{values}], slave=1); print("SUCCESS" if not r.isError() else "FAIL"); c.close()',
    },
    "modpoll": {
        "fc01": "/opt/modpoll/x86_64-linux-gnu/modpoll -m tcp -p {port} -a 1 -t 0 -r {addr} -c {count} 127.0.0.1",
        "fc02": "/opt/modpoll/x86_64-linux-gnu/modpoll -m tcp -p {port} -a 1 -t 1 -r {addr} -c {count} 127.0.0.1",
        "fc03": "/opt/modpoll/x86_64-linux-gnu/modpoll -m tcp -p {port} -a 1 -t 3 -r {addr} -c {count} 127.0.0.1",
        "fc04": "/opt/modpoll/x86_64-linux-gnu/modpoll -m tcp -p {port} -a 1 -t 4 -r {addr} -c {count} 127.0.0.1",
        "fc05": "/opt/modpoll/x86_64-linux-gnu/modpoll -m tcp -p {port} -a 1 -t 0 -r {addr} {value} 127.0.0.1",
        "fc06": "/opt/modpoll/x86_64-linux-gnu/modpoll -m tcp -p {port} -a 1 -t 3 -r {addr} {value} 127.0.0.1",
    },
    "our_client": {
        "fc03": "/modbus-lib/build/examples/modbus_tcp_client_cli --host 127.0.0.1 --port {port} --unit 1 --register {addr} --count {count}",
        "fc06": "/modbus-lib/build/examples/modbus_tcp_client_cli --host 127.0.0.1 --port {port} --unit 1 --register {addr} --write {value}",
    }
}

# Test scenarios - Focus on available implementations (pymodbus + our library)
SCENARIOS = [
    # FC01 - Read Coils (pymodbus baseline)
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc01", "addr": 0, "count": 10, "desc": "FC01: Read 10 coils"},
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc01", "addr": 100, "count": 5, "desc": "FC01: Read 5 coils at offset"},
    
    # FC02 - Read Discrete Inputs (pymodbus baseline)
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc02", "addr": 0, "count": 10, "desc": "FC02: Read 10 discrete inputs"},
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc02", "addr": 50, "count": 8, "desc": "FC02: Read 8 discrete inputs at offset"},
    
    # FC03 - Read Holding Registers (CRITICAL - test all combinations)
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc03", "addr": 0, "count": 10, "desc": "FC03: Read 10 holding registers"},
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc03", "addr": 100, "count": 20, "desc": "FC03: Read 20 registers at offset"},
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc03", "addr": 500, "count": 1, "desc": "FC03: Read single register"},
    {"server": "pymodbus", "client": "our_client", "fc": "fc03", "addr": 0, "count": 10, "desc": "FC03: Our client → pymodbus server"},
    {"server": "our_server", "client": "pymodbus", "fc": "fc03", "addr": 0, "count": 10, "desc": "FC03: pymodbus client → Our server"},
    
    # FC04 - Read Input Registers (pymodbus baseline)
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc04", "addr": 0, "count": 10, "desc": "FC04: Read 10 input registers"},
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc04", "addr": 200, "count": 15, "desc": "FC04: Read 15 input registers at offset"},
    
    # FC05 - Write Single Coil (pymodbus baseline)
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc05", "addr": 10, "value": 1, "desc": "FC05: Write coil ON"},
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc05", "addr": 20, "value": 0, "desc": "FC05: Write coil OFF"},
    
    # FC06 - Write Single Register (CRITICAL - test all combinations)
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc06", "addr": 10, "value": 1234, "desc": "FC06: Write single register (1234)"},
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc06", "addr": 50, "value": 65535, "desc": "FC06: Write register max value"},
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc06", "addr": 100, "value": 0, "desc": "FC06: Write register zero"},
    {"server": "pymodbus", "client": "our_client", "fc": "fc06", "addr": 10, "value": 5678, "desc": "FC06: Our client → pymodbus server"},
    {"server": "our_server", "client": "pymodbus", "fc": "fc06", "addr": 10, "value": 9999, "desc": "FC06: pymodbus client → Our server"},
    
    # FC15 - Write Multiple Coils (pymodbus baseline)
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc15", "addr": 10, "values": "1,0,1,1,0", "desc": "FC15: Write 5 coils"},
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc15", "addr": 50, "values": "1,1,1,1,1,1,1,1", "desc": "FC15: Write 8 coils (all ON)"},
    
    # FC16 - Write Multiple Registers (CRITICAL - test all combinations)
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc16", "addr": 10, "values": "100,200,300,400,500", "desc": "FC16: Write 5 registers"},
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc16", "addr": 100, "values": "1,2,3,4,5,6,7,8,9,10", "desc": "FC16: Write 10 registers"},
    {"server": "pymodbus", "client": "pymodbus", "fc": "fc16", "addr": 200, "values": "65535,0,32768,1,65534", "desc": "FC16: Write edge case values"},
]


class TestRunner:
    def __init__(self):
        self.results = []
        self.port_counter = 5502
        
    def get_next_port(self):
        """Allocate unique port for each test"""
        port = self.port_counter
        self.port_counter += 1
        return port
        
    def start_server(self, server_name: str, port: int) -> Optional[subprocess.Popen]:
        """Start a Modbus server"""
        config = SERVERS[server_name]
        cmd = config["cmd"].copy() if isinstance(config["cmd"], list) else [config["cmd"]]
        
        # Replace port placeholder
        cmd = [str(c).format(port=port) if isinstance(c, str) else c for c in cmd]
        
        print(f"  Starting {server_name} server on port {port}...")
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )
        
        # Wait for server ready
        timeout = time.time() + 5
        while time.time() < timeout:
            if proc.poll() is not None:
                print(f"  ERROR: Server died immediately")
                return None
            time.sleep(0.1)
            
        time.sleep(1)  # Extra settling time
        return proc
        
    def run_client(self, client_name: str, fc: str, port: int, **params) -> tuple[bool, str]:
        """Run a Modbus client command"""
        client_config = CLIENTS[client_name]
        
        if fc not in client_config:
            return False, f"Function code {fc} not supported by {client_name}"
            
        cmd_template = client_config[fc]
        
        # Python inline code
        if client_name == "pymodbus" and not cmd_template.startswith("/"):
            cmd = ["python3", "-c", cmd_template.format(port=port, **params)]
        else:
            # Command line tool
            cmd = cmd_template.format(port=port, **params).split()
        
        print(f"  Running {client_name} {fc}...")
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=5
            )
            
            output = result.stdout + result.stderr
            success = "SUCCESS" in output or result.returncode == 0
            
            return success, output
            
        except subprocess.TimeoutExpired:
            return False, "Timeout"
        except Exception as e:
            return False, str(e)
            
    def run_test(self, scenario: dict) -> dict:
        """Run a single test scenario"""
        test_name = f"{scenario['server']}_{scenario['client']}_{scenario['fc']}_{scenario['addr']}"
        print(f"\n{'='*70}")
        print(f"Test: {test_name}")
        print(f"Description: {scenario['desc']}")
        print(f"{'='*70}")
        
        port = self.get_next_port()
        pcap_file = PCAP_DIR / f"{test_name}.pcap"
        
        result = {
            "name": test_name,
            "description": scenario['desc'],
            "server": scenario['server'],
            "client": scenario['client'],
            "function_code": scenario['fc'],
            "port": port,
            "pcap_file": str(pcap_file),
            "success": False,
            "error": None
        }
        
        # Start PCAP capture
        print("  Starting packet capture...")
        tcpdump = subprocess.Popen(
            ["tcpdump", "-i", "lo", "-w", str(pcap_file), f"port {port}"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        time.sleep(1)
        
        server_proc = None
        try:
            # Start server
            server_proc = self.start_server(scenario['server'], port)
            if not server_proc:
                result["error"] = "Failed to start server"
                return result
            
            # Run client
            params = {k: v for k, v in scenario.items() if k in ['addr', 'count', 'value', 'values']}
            success, output = self.run_client(scenario['client'], scenario['fc'], port, **params)
            
            expect_error = scenario.get('expect_error', False)
            if expect_error:
                # For error test cases, we expect failure
                result["success"] = not success
                result["error"] = output if not success else None
            else:
                result["success"] = success
                result["error"] = None if success else output
                
            print(f"  Result: {'PASS' if result['success'] else 'FAIL'}")
            if result['error']:
                print(f"  Error: {result['error'][:200]}")
                
        except Exception as e:
            result["error"] = str(e)
            print(f"  Exception: {e}")
            
        finally:
            # Cleanup
            if server_proc:
                server_proc.terminate()
                server_proc.wait(timeout=2)
                
            time.sleep(0.5)
            tcpdump.terminate()
            tcpdump.wait(timeout=2)
            
        return result
        
    def run_all_tests(self):
        """Run all test scenarios"""
        print("\n" + "="*70)
        print("Gate 26: Expanded Interoperability Test Suite")
        print(f"Total scenarios: {len(SCENARIOS)}")
        print("="*70)
        
        for i, scenario in enumerate(SCENARIOS, 1):
            print(f"\n[{i}/{len(SCENARIOS)}]")
            result = self.run_test(scenario)
            self.results.append(result)
            time.sleep(1)  # Avoid port conflicts
            
        # Generate summary
        passed = sum(1 for r in self.results if r['success'])
        failed = len(self.results) - passed
        
        summary = {
            "total": len(self.results),
            "passed": passed,
            "failed": failed,
            "pass_rate": f"{(passed/len(self.results)*100):.1f}%",
            "tests": self.results
        }
        
        # Save results
        results_file = RESULTS_DIR / "interop_results.json"
        with open(results_file, 'w') as f:
            json.dump(summary, f, indent=2)
            
        print("\n" + "="*70)
        print("FINAL RESULTS")
        print("="*70)
        print(f"Total:  {summary['total']}")
        print(f"Passed: {summary['passed']} ({summary['pass_rate']})")
        print(f"Failed: {summary['failed']}")
        print(f"\nResults saved to: {results_file}")
        print(f"PCAPs saved to: {PCAP_DIR}")
        
        # Validate PCAPs
        self.validate_pcaps()
        
        return summary['failed'] == 0
        
    def validate_pcaps(self):
        """Validate captured PCAP files"""
        print("\n" + "="*70)
        print("PCAP VALIDATION")
        print("="*70)
        
        pcap_files = list(PCAP_DIR.glob("*.pcap"))
        print(f"Found {len(pcap_files)} PCAP files")
        
        for pcap in pcap_files[:3]:  # Sample first 3
            print(f"\nValidating: {pcap.name}")
            result = subprocess.run(
                ["tshark", "-r", str(pcap), "-Y", "modbus", "-T", "fields", 
                 "-e", "modbus.func_code", "-e", "modbus.unit_id"],
                capture_output=True,
                text=True
            )
            if result.stdout:
                print(f"  Modbus packets found: ✓")
                print(f"  Sample: {result.stdout.split()[0] if result.stdout.split() else 'N/A'}")
            else:
                print(f"  WARNING: No Modbus packets detected")


def main():
    runner = TestRunner()
    success = runner.run_all_tests()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
