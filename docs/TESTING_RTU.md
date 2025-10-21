# Testing Modbus RTU with Virtual Serial Ports

This guide explains how to test the Modbus RTU server and client using virtual serial ports created with `socat`.

## Prerequisites

- **socat** must be installed:
  ```bash
  # macOS
  brew install socat

  # Linux
  sudo apt-get install socat
  ```

## Quick Start

### Option 1: Using the Test Script (Recommended)

```bash
# Terminal 1: Start the server with virtual serial ports
./scripts/test_rtu_server.sh

# Terminal 2: Start the client
./build/examples/modbus_rtu_client --device /tmp/modbus_client --baud 9600 --unit 17
```

### Option 2: Manual Setup

```bash
# Terminal 1: Create virtual serial port pair
./scripts/create_virtual_serial.sh
# This creates /tmp/modbus_server and /tmp/modbus_client

# Terminal 2: Start the RTU server
./build/examples/modbus_rtu_server --device /tmp/modbus_server --baud 9600 --unit 17

# Terminal 3: Start the RTU client
./build/examples/modbus_rtu_client --device /tmp/modbus_client --baud 9600 --unit 17
```

## Using the RTU Client

Once the client is running, you can use these commands:

```
> read 0 10          # Read 10 holding registers starting at address 0
> write 5 1234       # Write value 1234 to register 5
> help               # Show available commands
> quit               # Exit
```

## Example Session

```bash
# Terminal 1 (Server)
$ ./scripts/test_rtu_server.sh
=== Modbus RTU Server Test ===

Step 1: Creating virtual serial ports
  Server port: /tmp/modbus_server
  Client port: /tmp/modbus_client
✓ Virtual serial ports created

Step 2: Starting RTU server
  Unit ID: 17 (0x11)
  Baud rate: 9600
  Device: /tmp/modbus_server

Press Ctrl+C to stop the server

----------------------------------------

Modbus RTU server ready (unit 0x11, baud 9600)
Waiting for master requests...

# Terminal 2 (Client)
$ ./build/examples/modbus_rtu_client --device /tmp/modbus_client --baud 9600 --unit 17
Modbus RTU client ready (unit 0x11, baud 9600)
Type 'help' for available commands

> read 0 5
Request sent (FC03: address=0, count=5)
Response received (5 registers):
  [0] = 0 (0x0000)
  [1] = 1 (0x0001)
  [2] = 2 (0x0002)
  [3] = 3 (0x0003)
  [4] = 4 (0x0004)

> write 10 999
Request sent (FC06: address=10, value=999)
Write confirmed

> read 10 1
Request sent (FC03: address=10, count=1)
Response received (1 registers):
  [0] = 999 (0x03E7)

> quit
Shutting down...
```

## Understanding the Virtual Serial Ports

`socat` creates two pseudo-terminals (PTYs) that are linked together:
- `/tmp/modbus_server` - Used by the RTU server
- `/tmp/modbus_client` - Used by the RTU client

Data written to one appears on the other, simulating a real RS-485 connection.

## Troubleshooting

### "Permission denied" when opening device

Make sure you have permission to access the device:
```bash
# Check device ownership
ls -l /tmp/modbus_*

# If needed, fix permissions
chmod 666 /tmp/modbus_*
```

### "Device not found"

Make sure the virtual serial ports are created before starting server/client:
```bash
# Check if devices exist
ls -l /tmp/modbus_*

# If not, start socat first
./scripts/create_virtual_serial.sh
```

### Testing with Real Serial Hardware

If you have USB-Serial adapters, you can test with real hardware:

```bash
# Find your devices
ls /dev/tty.*          # macOS
ls /dev/ttyUSB*        # Linux

# Connect two USB-Serial adapters with a null modem cable or jumper wires
# Then run:
./build/examples/modbus_rtu_server --device /dev/ttyUSB0 --baud 9600 --unit 17
./build/examples/modbus_rtu_client --device /dev/ttyUSB1 --baud 9600 --unit 17
```

## Supported Baud Rates

- 110, 300, 600, 1200, 2400, 4800, 9600 (default)
- 19200, 38400, 57600, 115200, 230400

## Supported Function Codes

### Server

- **FC03** - Read Holding Registers
- **FC06** - Write Single Register
- **FC16** - Write Multiple Registers

### Client

- **FC03** - Read Holding Registers
- **FC06** - Write Single Register
