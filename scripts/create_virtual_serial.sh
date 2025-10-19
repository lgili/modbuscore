#!/bin/bash
# Create a pair of virtual serial ports for testing Modbus RTU
# Usage: ./scripts/create_virtual_serial.sh

set -e

LINK1="/tmp/modbus_server"
LINK2="/tmp/modbus_client"

# Remove old links if they exist
rm -f "$LINK1" "$LINK2" 2>/dev/null || true

echo "Creating virtual serial port pair..."
echo "  Server side: $LINK1"
echo "  Client side: $LINK2"
echo ""
echo "Press Ctrl+C to stop"
echo ""

# Create linked pseudo-terminals with proper permissions
socat -d -d \
    PTY,raw,echo=0,link="$LINK1",mode=666 \
    PTY,raw,echo=0,link="$LINK2",mode=666
