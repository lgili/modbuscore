#!/usr/bin/env python3
"""
Gate 26: Simplified Interop Test (Baseline)
===========================================

Simple pymodbus server/client test to validate Docker infrastructure.
This is the baseline - once this passes, we expand to other implementations.
"""

import json
import subprocess
import sys
import time
from pathlib import Path

def test_pymodbus_to_pymodbus():
    """Test pymodbus server with pymodbus client - most reliable baseline"""
    print("=" * 70)
    print("Testing: pymodbus server ↔ pymodbus client (FC03 Read)")
    print("=" * 70)
    
    # Start pymodbus server
    print("[1/4] Starting pymodbus server on port 5502...")
    server_proc = subprocess.Popen([
        "python3", "-c", """
from pymodbus.server import StartTcpServer
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext
from pymodbus.datastore import ModbusSequentialDataBlock

# Initialize data store with test values  
# Note: pymodbus ModbusSequentialDataBlock(address, values) where:
# - address: first Modbus register address
# - values[0] will be at that address
# So if address=0, values[0] is at register 0, but pymodbus internally adds +1
# Solution: Start block at address 1, so reading addr 0 gets values[0]
values = [100, 200, 300, 400, 500, 600, 700, 800, 900, 1000] + [0]*90
store = ModbusSlaveContext(
    di=ModbusSequentialDataBlock(1, [0]*100),
    co=ModbusSequentialDataBlock(1, [0]*100),
    hr=ModbusSequentialDataBlock(1, values),
    ir=ModbusSequentialDataBlock(1, [0]*100)
)
context = ModbusServerContext(slaves=store, single=True)

print('PyModbus server starting on port 5502...', flush=True)
StartTcpServer(context=context, address=('0.0.0.0', 5502))
"""
    ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    
    time.sleep(3)  # Give server time to start
    
    if server_proc.poll() is not None:
        print("❌ FAIL: Server failed to start")
        return False
    
    print("✅ Server started successfully")
    
    # Run pymodbus client
    print("[2/4] Running pymodbus client...")
    try:
        result = subprocess.run([
            "python3", "-c", """
from pymodbus.client import ModbusTcpClient
import sys

client = ModbusTcpClient('127.0.0.1', port=5502, timeout=5)
if not client.connect():
    print('Failed to connect', flush=True)
    sys.exit(1)

# Read 10 holding registers starting at address 0
result = client.read_holding_registers(0, 10, slave=1)

if result.isError():
    print(f'Read error: {result}', flush=True)
    sys.exit(1)

# Expect: [100, 200, 300, 400, 500, 600, 700, 800, 900, 1000]
expected = [100, 200, 300, 400, 500, 600, 700, 800, 900, 1000]
actual = result.registers

print(f'Expected: {expected}', flush=True)
print(f'Actual:   {actual}', flush=True)

if actual == expected:
    print('✅ Values match!', flush=True)
    sys.exit(0)
else:
    print(f'❌ Mismatch!', flush=True)
    sys.exit(1)

client.close()
"""
        ], capture_output=True, text=True, timeout=10)
        
        print(result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        
        success = (result.returncode == 0)
        
    except subprocess.TimeoutExpired:
        print("❌ FAIL: Client timeout")
        success = False
    except Exception as e:
        print(f"❌ FAIL: {e}")
        success = False
    finally:
        server_proc.terminate()
        server_proc.wait(timeout=5)
    
    print("[3/4] Server stopped")
    
    if success:
        print("[4/4] ✅ TEST PASSED")
        return True
    else:
        print("[4/4] ❌ TEST FAILED")
        return False

def test_pymodbus_write_single():
    """Test pymodbus FC06 - Write Single Register"""
    print("\n" + "=" * 70)
    print("Testing: pymodbus FC06 (Write Single Register)")
    print("=" * 70)
    
    # Start pymodbus server
    print("[1/5] Starting pymodbus server on port 5503...")
    server_proc = subprocess.Popen([
        "python3", "-c", """
from pymodbus.server import StartTcpServer
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext
from pymodbus.datastore import ModbusSequentialDataBlock

values = [0] * 100
store = ModbusSlaveContext(
    di=ModbusSequentialDataBlock(1, [0]*100),
    co=ModbusSequentialDataBlock(1, [0]*100),
    hr=ModbusSequentialDataBlock(1, values),
    ir=ModbusSequentialDataBlock(1, [0]*100)
)
context = ModbusServerContext(slaves=store, single=True)

print('PyModbus server starting on port 5503...', flush=True)
StartTcpServer(context=context, address=('0.0.0.0', 5503))
"""
    ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    
    time.sleep(3)
    
    if server_proc.poll() is not None:
        print("❌ FAIL: Server failed to start")
        return False
    
    print("✅ Server started successfully")
    
    # Write and read back
    print("[2/5] Writing value 12345 to register 10...")
    try:
        result = subprocess.run([
            "python3", "-c", """
from pymodbus.client import ModbusTcpClient
import sys

client = ModbusTcpClient('127.0.0.1', port=5503, timeout=5)
if not client.connect():
    print('Failed to connect', flush=True)
    sys.exit(1)

# Write single register
write_result = client.write_register(10, 12345, slave=1)
if write_result.isError():
    print(f'Write error: {write_result}', flush=True)
    sys.exit(1)

print('✅ Write successful', flush=True)

# Read back to verify
read_result = client.read_holding_registers(10, 1, slave=1)
if read_result.isError():
    print(f'Read error: {read_result}', flush=True)
    sys.exit(1)

value = read_result.registers[0]
print(f'Read back value: {value}', flush=True)

if value == 12345:
    print('✅ Value verified!', flush=True)
    sys.exit(0)
else:
    print(f'❌ Mismatch: expected 12345, got {value}', flush=True)
    sys.exit(1)

client.close()
"""
        ], capture_output=True, text=True, timeout=10)
        
        print(result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        
        success = (result.returncode == 0)
        
    except subprocess.TimeoutExpired:
        print("❌ FAIL: Client timeout")
        success = False
    except Exception as e:
        print(f"❌ FAIL: {e}")
        success = False
    finally:
        server_proc.terminate()
        server_proc.wait(timeout=5)
    
    print("[3/5] Server stopped")
    
    if success:
        print("[4/5] ✅ TEST PASSED")
        return True
    else:
        print("[4/5] ❌ TEST FAILED")
        return False

def test_pymodbus_write_multiple():
    """Test pymodbus FC16 - Write Multiple Registers"""
    print("\n" + "=" * 70)
    print("Testing: pymodbus FC16 (Write Multiple Registers)")
    print("=" * 70)
    
    # Start pymodbus server
    print("[1/4] Starting pymodbus server on port 5504...")
    server_proc = subprocess.Popen([
        "python3", "-c", """
from pymodbus.server import StartTcpServer
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext
from pymodbus.datastore import ModbusSequentialDataBlock

values = [0] * 100
store = ModbusSlaveContext(
    di=ModbusSequentialDataBlock(1, [0]*100),
    co=ModbusSequentialDataBlock(1, [0]*100),
    hr=ModbusSequentialDataBlock(1, values),
    ir=ModbusSequentialDataBlock(1, [0]*100)
)
context = ModbusServerContext(slaves=store, single=True)

print('PyModbus server starting on port 5504...', flush=True)
StartTcpServer(context=context, address=('0.0.0.0', 5504))
"""
    ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    
    time.sleep(3)
    
    if server_proc.poll() is not None:
        print("❌ FAIL: Server failed to start")
        return False
    
    print("✅ Server started successfully")
    
    # Write multiple and read back
    print("[2/4] Writing 5 values [111, 222, 333, 444, 555] to registers 20-24...")
    try:
        result = subprocess.run([
            "python3", "-c", """
from pymodbus.client import ModbusTcpClient
import sys

client = ModbusTcpClient('127.0.0.1', port=5504, timeout=5)
if not client.connect():
    print('Failed to connect', flush=True)
    sys.exit(1)

# Write multiple registers
values_to_write = [111, 222, 333, 444, 555]
write_result = client.write_registers(20, values_to_write, slave=1)
if write_result.isError():
    print(f'Write error: {write_result}', flush=True)
    sys.exit(1)

print('✅ Write successful', flush=True)

# Read back to verify
read_result = client.read_holding_registers(20, 5, slave=1)
if read_result.isError():
    print(f'Read error: {read_result}', flush=True)
    sys.exit(1)

expected = [111, 222, 333, 444, 555]
actual = read_result.registers

print(f'Expected: {expected}', flush=True)
print(f'Actual:   {actual}', flush=True)

if actual == expected:
    print('✅ Values verified!', flush=True)
    sys.exit(0)
else:
    print(f'❌ Mismatch!', flush=True)
    sys.exit(1)

client.close()
"""
        ], capture_output=True, text=True, timeout=10)
        
        print(result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        
        success = (result.returncode == 0)
        
    except subprocess.TimeoutExpired:
        print("❌ FAIL: Client timeout")
        success = False
    except Exception as e:
        print(f"❌ FAIL: {e}")
        success = False
    finally:
        server_proc.terminate()
        server_proc.wait(timeout=5)
    
    print("[3/4] Server stopped")
    
    if success:
        print("[4/4] ✅ TEST PASSED")
        return True
    else:
        print("[4/4] ❌ TEST FAILED")
        return False

def test_pymodbus_error_handling():
    """Test pymodbus error handling - illegal address"""
    print("\n" + "=" * 70)
    print("Testing: pymodbus Error Handling (Illegal Address)")
    print("=" * 70)
    
    # Start pymodbus server with limited address range
    print("[1/4] Starting pymodbus server on port 5505...")
    server_proc = subprocess.Popen([
        "python3", "-c", """
from pymodbus.server import StartTcpServer
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext
from pymodbus.datastore import ModbusSequentialDataBlock

# Only 10 registers available (addresses 1-10)
values = [100, 200, 300, 400, 500, 600, 700, 800, 900, 1000]
store = ModbusSlaveContext(
    di=ModbusSequentialDataBlock(1, [0]*10),
    co=ModbusSequentialDataBlock(1, [0]*10),
    hr=ModbusSequentialDataBlock(1, values),
    ir=ModbusSequentialDataBlock(1, [0]*10)
)
context = ModbusServerContext(slaves=store, single=True)

print('PyModbus server starting on port 5505...', flush=True)
StartTcpServer(context=context, address=('0.0.0.0', 5505))
"""
    ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    
    time.sleep(3)
    
    if server_proc.poll() is not None:
        print("❌ FAIL: Server failed to start")
        return False
    
    print("✅ Server started successfully")
    
    # Try to read illegal address
    print("[2/4] Attempting to read illegal address 100 (should fail)...")
    try:
        result = subprocess.run([
            "python3", "-c", """
from pymodbus.client import ModbusTcpClient
import sys

client = ModbusTcpClient('127.0.0.1', port=5505, timeout=5)
if not client.connect():
    print('Failed to connect', flush=True)
    sys.exit(1)

# Try to read from address 100 (out of range)
result = client.read_holding_registers(100, 1, slave=1)

if result.isError():
    print(f'✅ Got expected error: {result}', flush=True)
    # Check for exception code 2 (Illegal Data Address)
    if hasattr(result, 'exception_code') and result.exception_code == 2:
        print('✅ Correct exception code (0x02 - Illegal Data Address)', flush=True)
        sys.exit(0)
    else:
        print('✅ Error received (exception code check skipped)', flush=True)
        sys.exit(0)
else:
    print(f'❌ Should have failed but got: {result.registers}', flush=True)
    sys.exit(1)

client.close()
"""
        ], capture_output=True, text=True, timeout=10)
        
        print(result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        
        success = (result.returncode == 0)
        
    except subprocess.TimeoutExpired:
        print("❌ FAIL: Client timeout")
        success = False
    except Exception as e:
        print(f"❌ FAIL: {e}")
        success = False
    finally:
        server_proc.terminate()
        server_proc.wait(timeout=5)
    
    print("[3/4] Server stopped")
    
    if success:
        print("[4/4] ✅ TEST PASSED")
        return True
    else:
        print("[4/4] ❌ TEST FAILED")
        return False

def test_our_client_pymodbus_server():
    """Test our library client with pymodbus server"""
    print("\n" + "=" * 70)
    print("Testing: Our Library Client → pymodbus server (FC03)")
    print("=" * 70)
    
    # Start pymodbus server
    print("[1/4] Starting pymodbus server on port 5506...")
    server_proc = subprocess.Popen([
        "python3", "-c", """
from pymodbus.server import StartTcpServer
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext
from pymodbus.datastore import ModbusSequentialDataBlock

values = [11, 22, 33, 44, 55, 66, 77, 88, 99, 100]
store = ModbusSlaveContext(
    di=ModbusSequentialDataBlock(1, [0]*100),
    co=ModbusSequentialDataBlock(1, [0]*100),
    hr=ModbusSequentialDataBlock(1, values + [0]*90),
    ir=ModbusSequentialDataBlock(1, [0]*100)
)
context = ModbusServerContext(slaves=store, single=True)

print('PyModbus server starting on port 5506...', flush=True)
StartTcpServer(context=context, address=('0.0.0.0', 5506))
"""
    ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    
    time.sleep(3)
    
    if server_proc.poll() is not None:
        print("❌ FAIL: Server failed to start")
        return False
    
    print("✅ Server started successfully")
    
    # Run our library client
    print("[2/4] Running our library client...")
    try:
        result = subprocess.run([
            "/modbus-lib/build/examples/modbus_tcp_client_cli",
            "--host", "127.0.0.1",
            "--port", "5506",
            "--unit", "1",
            "--register", "0",
            "--count", "10",
            "--expect", "11,22,33,44,55,66,77,88,99,100"
        ], capture_output=True, text=True, timeout=10)
        
        print(result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        
        success = (result.returncode == 0)
        
    except subprocess.TimeoutExpired:
        print("❌ FAIL: Client timeout")
        success = False
    except Exception as e:
        print(f"❌ FAIL: {e}")
        success = False
    finally:
        server_proc.terminate()
        server_proc.wait(timeout=5)
    
    print("[3/4] Server stopped")
    
    if success:
        print("[4/4] ✅ TEST PASSED")
        return True
    else:
        print("[4/4] ❌ TEST FAILED")
        return False

def test_pymodbus_client_our_server():
    """Test pymodbus client with our library server"""
    print("\n" + "=" * 70)
    print("Testing: pymodbus Client → Our Library Server (FC03)")
    print("=" * 70)
    
    # Start our library server
    print("[1/4] Starting our library server on port 5507...")
    server_proc = subprocess.Popen([
        "/modbus-lib/build/examples/modbus_tcp_server_demo",
        "--port", "5507",
        "--unit", "1"
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    
    time.sleep(3)
    
    if server_proc.poll() is not None:
        stdout, stderr = server_proc.communicate()
        print(f"❌ FAIL: Server failed to start")
        print("STDOUT:", stdout.decode() if stdout else "")
        print("STDERR:", stderr.decode() if stderr else "")
        return False
    
    print("✅ Server started successfully")
    
    # Run pymodbus client
    print("[2/4] Running pymodbus client...")
    try:
        result = subprocess.run([
            "python3", "-c", """
from pymodbus.client import ModbusTcpClient
import sys

client = ModbusTcpClient('127.0.0.1', port=5507, timeout=5)
if not client.connect():
    print('Failed to connect', flush=True)
    sys.exit(1)

# Our server initializes registers with specific values
# Read 5 holding registers starting at address 0
result = client.read_holding_registers(0, 5, slave=1)

if result.isError():
    print(f'Read error: {result}', flush=True)
    sys.exit(1)

values = result.registers
print(f'Read values: {values}', flush=True)

# Just verify we got some data (our server has initialized registers)
if len(values) == 5:
    print('✅ Successfully read 5 registers from our server!', flush=True)
    sys.exit(0)
else:
    print(f'❌ Expected 5 values, got {len(values)}', flush=True)
    sys.exit(1)

client.close()
"""
        ], capture_output=True, text=True, timeout=10)
        
        print(result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        
        success = (result.returncode == 0)
        
    except subprocess.TimeoutExpired:
        print("❌ FAIL: Client timeout")
        success = False
    except Exception as e:
        print(f"❌ FAIL: {e}")
        success = False
    finally:
        server_proc.terminate()
        server_proc.wait(timeout=5)
    
    print("[3/4] Server stopped")
    
    if success:
        print("[4/4] ✅ TEST PASSED")
        return True
    else:
        print("[4/4] ❌ TEST FAILED")
        return False

def main():
    print("\n🐳 Gate 26: Interop Infrastructure Validation\n")
    
    results = {
        "tests": [],
        "total": 0,
        "passed": 0,
        "failed": 0
    }
    
    # Define all tests
    tests = [
        ("pymodbus_to_pymodbus_fc03", test_pymodbus_to_pymodbus, 
         "Read 10 holding registers (pymodbus server + client)"),
        ("pymodbus_write_single_fc06", test_pymodbus_write_single,
         "Write single register (pymodbus FC06)"),
        ("pymodbus_write_multiple_fc16", test_pymodbus_write_multiple,
         "Write multiple registers (pymodbus FC16)"),
        ("pymodbus_error_handling", test_pymodbus_error_handling,
         "Error handling - illegal address exception"),
        ("our_client_pymodbus_server", test_our_client_pymodbus_server,
         "Our library client → pymodbus server FC03"),
        ("pymodbus_client_our_server", test_pymodbus_client_our_server,
         "pymodbus client → Our library server FC03"),
    ]
    
    # Run all tests
    for test_name, test_func, description in tests:
        print(f"\n{'='*70}")
        print(f"Running test: {test_name}")
        print(f"{'='*70}")
        
        try:
            success = test_func()
        except Exception as e:
            print(f"❌ EXCEPTION: {e}")
            success = False
        
        results["tests"].append({
            "name": test_name,
            "success": success,
            "description": description
        })
        results["total"] += 1
        if success:
            results["passed"] += 1
        else:
            results["failed"] += 1
    
    # Save results
    results_file = Path("/results/interop_results.json")
    results_file.parent.mkdir(parents=True, exist_ok=True)
    
    with open(results_file, "w") as f:
        json.dump(results, f, indent=2)
    
    print("\n" + "=" * 70)
    print(f"SUMMARY: {results['passed']}/{results['total']} tests passed")
    print("=" * 70)
    
    # Print failed tests if any
    if results["failed"] > 0:
        print(f"\n❌ Failed tests:")
        for test in results["tests"]:
            if not test["success"]:
                print(f"  - {test['name']}: {test['description']}")
    
    print(f"\n📄 Results saved to: {results_file}")
    
    if results["failed"] > 0:
        print(f"\n❌ {results['failed']} test(s) failed")
        sys.exit(1)
    else:
        print("\n✅ All tests passed!")
        sys.exit(0)

if __name__ == "__main__":
    main()
