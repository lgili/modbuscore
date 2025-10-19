#!/usr/bin/env python3
"""
Integration test runner - starts server, runs client, checks result
"""
import subprocess
import time
import signal
import sys
import os

def kill_port_5502():
    """Kill any process using port 5502"""
    try:
        result = subprocess.run(['lsof', '-ti:5502'], capture_output=True, text=True)
        if result.stdout.strip():
            pids = result.stdout.strip().split('\n')
            for pid in pids:
                try:
                    os.kill(int(pid), signal.SIGKILL)
                    print(f"Killed process {pid} on port 5502")
                except:
                    pass
    except:
        pass

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    build_dir = os.path.join(project_root, 'build')
    client_path = os.path.join(build_dir, 'tests', 'example_tcp_client_fc03')
    server_path = os.path.join(script_dir, 'simple_tcp_server.py')

    print("=== ModbusCore v1.0 - Integration Test ===\n")

    # Check if client exists
    if not os.path.exists(client_path):
        print(f"ERROR: Client not found at {client_path}")
        print("Run: cmake --build build")
        return 1

    # Kill any existing server on port 5502
    print("Step 0: Clearing port 5502...")
    kill_port_5502()
    time.sleep(1)

    # Start server
    print("Step 1: Starting simple Modbus TCP server...")
    server_proc = subprocess.Popen(
        ['python3', server_path],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True
    )

    # Wait for server to start
    time.sleep(2)

    # Check if server is running
    if server_proc.poll() is not None:
        print("ERROR: Failed to start server")
        print(server_proc.stdout.read())
        return 1

    print(f"✓ Server running (PID={server_proc.pid})\n")

    try:
        # Run client
        print("Step 2: Running TCP client example...")
        print("=" * 40)

        client_proc = subprocess.run(
            [client_path],
            capture_output=True,
            text=True,
            timeout=10
        )

        print(client_proc.stdout)
        if client_proc.stderr:
            print(client_proc.stderr)

        print("=" * 40)
        print()

        test_result = "PASS" if client_proc.returncode == 0 else "FAIL"

    except subprocess.TimeoutExpired:
        print("ERROR: Client timed out")
        test_result = "FAIL"

    finally:
        # Stop server
        print("Step 3: Stopping server...")
        server_proc.terminate()
        try:
            server_proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server_proc.kill()
        print("✓ Server stopped\n")

    # Print result
    if test_result == "PASS":
        print("=" * 40)
        print("   ✓✓✓ INTEGRATION TEST PASSED ✓✓✓")
        print("=" * 40)
        return 0
    else:
        print("=" * 40)
        print("   ✗✗✗ INTEGRATION TEST FAILED ✗✗✗")
        print("=" * 40)
        return 1

if __name__ == '__main__':
    sys.exit(main())
