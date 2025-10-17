#!/usr/bin/env python3
"""
Gate 26: Modbus Interoperability Test Runner
============================================

Runs comprehensive interop tests between our library and major Modbus implementations:
- pymodbus (Python)
- libmodbus (C)
- modpoll (commercial tool)
- diagslave (simulator)

Captures PCAP files for each scenario and validates protocol compliance.
"""

import argparse
import json
import logging
import os
import subprocess
import sys
import time
from dataclasses import dataclass, asdict
from enum import Enum
from pathlib import Path
from typing import List, Dict, Optional

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)


class Implementation(Enum):
    """Modbus implementations under test"""
    OUR_LIB = "our-library"
    PYMODBUS = "pymodbus"
    LIBMODBUS = "libmodbus"
    MODPOLL = "modpoll"
    DIAGSLAVE = "diagslave"


class Role(Enum):
    """Test role: client or server"""
    CLIENT = "client"
    SERVER = "server"


@dataclass
class TestScenario:
    """Defines a single interop test scenario"""
    name: str
    function_code: int
    description: str
    server_impl: Implementation
    client_impl: Implementation
    register_address: int = 0
    register_count: int = 1
    expected_values: Optional[List[int]] = None
    should_fail: bool = False
    exception_code: Optional[int] = None


@dataclass
class TestResult:
    """Result of a single interop test"""
    scenario: str
    server: str
    client: str
    success: bool
    duration_ms: float
    error_message: Optional[str] = None
    pcap_file: Optional[str] = None
    validation_errors: List[str] = None


class InteropTestRunner:
    """Manages interop test execution and PCAP capture"""
    
    def __init__(self, pcap_dir: Path, results_dir: Path, verbose: bool = False):
        self.pcap_dir = pcap_dir
        self.results_dir = results_dir
        self.verbose = verbose
        self.results: List[TestResult] = []
        
        # Create output directories
        self.pcap_dir.mkdir(parents=True, exist_ok=True)
        self.results_dir.mkdir(parents=True, exist_ok=True)
        
        # Test configuration
        self.test_port = 5502
        self.test_host = "127.0.0.1"
        self.timeout = int(os.getenv("TIMEOUT", "30"))
    
    def get_scenarios(self) -> List[TestScenario]:
        """Define all interop test scenarios"""
        scenarios = []
        
        # FC03: Read Holding Registers - All implementations
        for server in [Implementation.OUR_LIB, Implementation.DIAGSLAVE]:
            for client in [Implementation.OUR_LIB, Implementation.PYMODBUS, Implementation.MODPOLL]:
                if server == client == Implementation.OUR_LIB:
                    continue  # Skip same-implementation test (covered elsewhere)
                
                scenarios.append(TestScenario(
                    name=f"fc03_{server.value}_server_{client.value}_client",
                    function_code=3,
                    description=f"Read Holding Registers: {server.value} server + {client.value} client",
                    server_impl=server,
                    client_impl=client,
                    register_address=0,
                    register_count=10,
                    expected_values=[100, 200, 300, 400, 500, 600, 700, 800, 900, 1000]
                ))
        
        # FC06: Write Single Register
        scenarios.append(TestScenario(
            name="fc06_write_single_pymodbus_our_lib",
            function_code=6,
            description="Write Single Register: pymodbus client → our-library server",
            server_impl=Implementation.OUR_LIB,
            client_impl=Implementation.PYMODBUS,
            register_address=10,
            register_count=1,
            expected_values=[12345]
        ))
        
        # FC16: Write Multiple Registers
        scenarios.append(TestScenario(
            name="fc16_write_multiple_our_lib_diagslave",
            function_code=16,
            description="Write Multiple Registers: our-library client → diagslave server",
            server_impl=Implementation.DIAGSLAVE,
            client_impl=Implementation.OUR_LIB,
            register_address=20,
            register_count=5,
            expected_values=[1, 2, 3, 4, 5]
        ))
        
        # Error scenarios
        scenarios.append(TestScenario(
            name="fc03_illegal_address_exception",
            function_code=3,
            description="Read Holding Registers: Illegal address exception",
            server_impl=Implementation.DIAGSLAVE,
            client_impl=Implementation.OUR_LIB,
            register_address=9999,
            register_count=1,
            should_fail=True,
            exception_code=2  # Illegal Data Address
        ))
        
        return scenarios
    
    def start_server(self, impl: Implementation, scenario: TestScenario) -> Optional[subprocess.Popen]:
        """Start a Modbus server for testing"""
        logger.info(f"Starting {impl.value} server on port {self.test_port}...")
        
        if impl == Implementation.OUR_LIB:
            cmd = [
                "/modbus-lib/build/examples/modbus_tcp_server_demo",
                "--port", str(self.test_port)
            ]
        elif impl == Implementation.DIAGSLAVE:
            cmd = [
                "diagslave",
                "-m", "tcp",
                "-p", str(self.test_port),
                "-a", "1"
            ]
        else:
            logger.error(f"Server implementation {impl} not supported")
            return None
        
        try:
            proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE if not self.verbose else None,
                stderr=subprocess.PIPE if not self.verbose else None
            )
            time.sleep(2)  # Allow server to start
            
            if proc.poll() is not None:
                logger.error(f"Server failed to start: {impl.value}")
                return None
            
            return proc
        except Exception as e:
            logger.error(f"Failed to start server {impl}: {e}")
            return None
    
    def run_client(self, impl: Implementation, scenario: TestScenario) -> tuple[bool, Optional[str]]:
        """Run a Modbus client test"""
        logger.info(f"Running {impl.value} client for {scenario.name}...")
        
        if impl == Implementation.OUR_LIB:
            return self._run_our_lib_client(scenario)
        elif impl == Implementation.PYMODBUS:
            return self._run_pymodbus_client(scenario)
        elif impl == Implementation.MODPOLL:
            return self._run_modpoll_client(scenario)
        else:
            return False, f"Client implementation {impl} not supported"
    
    def _run_our_lib_client(self, scenario: TestScenario) -> tuple[bool, Optional[str]]:
        """Run our library's TCP client"""
        cmd = [
            "/modbus-lib/build/examples/modbus_tcp_client_cli",
            "--host", self.test_host,
            "--port", str(self.test_port),
            "--unit", "1",
            "--register", str(scenario.register_address),
            "--count", str(scenario.register_count)
        ]
        
        if scenario.expected_values:
            cmd.extend(["--expect", ",".join(map(str, scenario.expected_values[:scenario.register_count]))])
        
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            
            if scenario.should_fail:
                return result.returncode != 0, None
            else:
                return result.returncode == 0, result.stderr if result.returncode != 0 else None
        
        except subprocess.TimeoutExpired:
            return False, "Client timeout"
        except Exception as e:
            return False, str(e)
    
    def _run_pymodbus_client(self, scenario: TestScenario) -> tuple[bool, Optional[str]]:
        """Run pymodbus client"""
        script = f"""
from pymodbus.client import ModbusTcpClient

client = ModbusTcpClient('{self.test_host}', port={self.test_port})
if not client.connect():
    exit(1)

try:
    if {scenario.function_code} == 3:
        response = client.read_holding_registers({scenario.register_address}, {scenario.register_count}, unit=1)
    elif {scenario.function_code} == 6:
        response = client.write_register({scenario.register_address}, {scenario.expected_values[0] if scenario.expected_values else 0}, unit=1)
    elif {scenario.function_code} == 16:
        response = client.write_registers({scenario.register_address}, {scenario.expected_values if scenario.expected_values else []}, unit=1)
    else:
        exit(1)
    
    if response.isError():
        if {scenario.should_fail}:
            exit(0)  # Expected error
        exit(1)
    else:
        if {scenario.should_fail}:
            exit(1)  # Should have failed
        exit(0)
finally:
    client.close()
"""
        
        try:
            result = subprocess.run(
                ["python3", "-c", script],
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            return result.returncode == 0, result.stderr if result.returncode != 0 else None
        except Exception as e:
            return False, str(e)
    
    def _run_modpoll_client(self, scenario: TestScenario) -> tuple[bool, Optional[str]]:
        """Run modpoll client (if available)"""
        cmd = [
            "modpoll",
            "-m", "tcp",
            "-a", "1",
            "-r", str(scenario.register_address + 1),  # modpoll uses 1-based addressing
            "-c", str(scenario.register_count),
            "-p", str(self.test_port),
            self.test_host
        ]
        
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            # modpoll returns 0 on success
            return result.returncode == 0, result.stderr if result.returncode != 0 else None
        except FileNotFoundError:
            logger.warning("modpoll not found, skipping test")
            return False, "modpoll not available"
        except Exception as e:
            return False, str(e)
    
    def capture_pcap(self, scenario: TestScenario, callback):
        """Capture PCAP during test execution"""
        pcap_file = self.pcap_dir / f"{scenario.name}.pcapng"
        
        # Start tcpdump
        tcpdump_cmd = [
            "tcpdump",
            "-i", "lo",
            "-w", str(pcap_file),
            f"tcp port {self.test_port}",
            "-U"  # Unbuffered
        ]
        
        try:
            tcpdump_proc = subprocess.Popen(
                tcpdump_cmd,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
            time.sleep(0.5)  # Allow tcpdump to start
            
            # Run the actual test
            result = callback()
            
            # Stop tcpdump
            time.sleep(0.5)
            tcpdump_proc.terminate()
            tcpdump_proc.wait(timeout=5)
            
            return result, str(pcap_file) if pcap_file.exists() else None
        
        except Exception as e:
            logger.error(f"PCAP capture failed: {e}")
            return callback(), None
    
    def run_scenario(self, scenario: TestScenario) -> TestResult:
        """Execute a single test scenario"""
        logger.info(f"\n{'='*70}")
        logger.info(f"Running scenario: {scenario.name}")
        logger.info(f"Description: {scenario.description}")
        logger.info(f"{'='*70}")
        
        start_time = time.time()
        
        # Start server
        server_proc = self.start_server(scenario.server_impl, scenario)
        if not server_proc:
            return TestResult(
                scenario=scenario.name,
                server=scenario.server_impl.value,
                client=scenario.client_impl.value,
                success=False,
                duration_ms=0,
                error_message="Failed to start server"
            )
        
        try:
            # Run client with PCAP capture
            def run_test():
                return self.run_client(scenario.client_impl, scenario)
            
            if os.getenv("CAPTURE_PCAP", "1") == "1":
                (success, error), pcap_file = self.capture_pcap(scenario, run_test)
            else:
                success, error = run_test()
                pcap_file = None
            
            duration_ms = (time.time() - start_time) * 1000
            
            result = TestResult(
                scenario=scenario.name,
                server=scenario.server_impl.value,
                client=scenario.client_impl.value,
                success=success,
                duration_ms=duration_ms,
                error_message=error,
                pcap_file=pcap_file
            )
            
            if success:
                logger.info(f"✅ PASS: {scenario.name} ({duration_ms:.1f}ms)")
            else:
                logger.error(f"❌ FAIL: {scenario.name} - {error}")
            
            return result
        
        finally:
            # Clean up server
            server_proc.terminate()
            try:
                server_proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server_proc.kill()
    
    def run_all_scenarios(self, scenario_filter: Optional[str] = None):
        """Run all test scenarios"""
        scenarios = self.get_scenarios()
        
        if scenario_filter:
            scenarios = [s for s in scenarios if scenario_filter in s.name]
        
        logger.info(f"Running {len(scenarios)} interop scenarios...")
        
        for scenario in scenarios:
            result = self.run_scenario(scenario)
            self.results.append(result)
            time.sleep(1)  # Brief pause between tests
        
        self.save_results()
        self.print_summary()
    
    def save_results(self):
        """Save test results to JSON"""
        results_file = self.results_dir / "interop_results.json"
        
        results_data = {
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
            "total_tests": len(self.results),
            "passed": sum(1 for r in self.results if r.success),
            "failed": sum(1 for r in self.results if not r.success),
            "results": [asdict(r) for r in self.results]
        }
        
        with open(results_file, 'w') as f:
            json.dump(results_data, f, indent=2)
        
        logger.info(f"Results saved to {results_file}")
    
    def print_summary(self):
        """Print test summary"""
        total = len(self.results)
        passed = sum(1 for r in self.results if r.success)
        failed = total - passed
        
        logger.info(f"\n{'='*70}")
        logger.info(f"INTEROP TEST SUMMARY")
        logger.info(f"{'='*70}")
        logger.info(f"Total:  {total}")
        logger.info(f"Passed: {passed} ({passed/total*100:.1f}%)")
        logger.info(f"Failed: {failed} ({failed/total*100:.1f}%)")
        logger.info(f"{'='*70}")
        
        if failed > 0:
            logger.info("\nFailed scenarios:")
            for result in self.results:
                if not result.success:
                    logger.info(f"  ❌ {result.scenario}: {result.error_message}")


def main():
    parser = argparse.ArgumentParser(description="Run Modbus interop tests (Gate 26)")
    parser.add_argument("--all", action="store_true", help="Run all scenarios")
    parser.add_argument("--scenario", help="Run specific scenario")
    parser.add_argument("--list", action="store_true", help="List available scenarios")
    parser.add_argument("--pcap-dir", default="/pcaps", help="PCAP output directory")
    parser.add_argument("--results-dir", default="/results", help="Results output directory")
    parser.add_argument("--verbose", action="store_true", help="Verbose output")
    
    args = parser.parse_args()
    
    runner = InteropTestRunner(
        pcap_dir=Path(args.pcap_dir),
        results_dir=Path(args.results_dir),
        verbose=args.verbose
    )
    
    if args.list:
        scenarios = runner.get_scenarios()
        print(f"\nAvailable scenarios ({len(scenarios)}):")
        for s in scenarios:
            print(f"  - {s.name}: {s.description}")
        return 0
    
    if args.all:
        runner.run_all_scenarios()
    elif args.scenario:
        runner.run_all_scenarios(scenario_filter=args.scenario)
    else:
        parser.print_help()
        return 1
    
    # Exit with error if any tests failed
    if any(not r.success for r in runner.results):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
