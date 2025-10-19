#!/bin/bash
# Test script for Modbus RTU server
# This script starts the virtual serial ports and the RTU server

set -e

cd "$(dirname "$0")/.."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Modbus RTU Server Test ===${NC}"
echo ""

# Check if build exists
if [ ! -f "build/examples/modbus_rtu_server" ]; then
    echo -e "${RED}Error: build/examples/modbus_rtu_server not found${NC}"
    echo "Please build the project first:"
    echo "  mkdir -p build && cd build && cmake .. && cmake --build ."
    exit 1
fi

# Check if socat is installed
if ! command -v socat &> /dev/null; then
    echo -e "${RED}Error: socat is not installed${NC}"
    echo "Install with: brew install socat"
    exit 1
fi

LINK1="/tmp/modbus_server"
LINK2="/tmp/modbus_client"

# Clean up old links
rm -f "$LINK1" "$LINK2" 2>/dev/null || true

# Function to cleanup on exit
cleanup() {
    echo ""
    echo -e "${YELLOW}Cleaning up...${NC}"
    rm -f "$LINK1" "$LINK2" 2>/dev/null || true
    pkill -P $$ 2>/dev/null || true
    jobs -p | xargs kill 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo -e "${GREEN}Step 1: Creating virtual serial ports${NC}"
echo "  Server port: $LINK1"
echo "  Client port: $LINK2"

# Start socat in background with proper permissions
socat -d -d \
    PTY,raw,echo=0,link="$LINK1",mode=666 \
    PTY,raw,echo=0,link="$LINK2",mode=666 &

SOCAT_PID=$!
sleep 2

# Check if socat is still running
if ! kill -0 $SOCAT_PID 2>/dev/null; then
    echo -e "${RED}Error: Failed to create virtual serial ports${NC}"
    exit 1
fi

# Verify links were created
if [ ! -L "$LINK1" ] || [ ! -L "$LINK2" ]; then
    echo -e "${RED}Error: Symbolic links were not created${NC}"
    echo "Expected: $LINK1 and $LINK2"
    ls -la /tmp/modbus_* 2>/dev/null || echo "No modbus_* files found in /tmp"
    exit 1
fi

echo -e "${GREEN}✓ Virtual serial ports created${NC}"
echo "  $LINK1 -> $(readlink $LINK1)"
echo "  $LINK2 -> $(readlink $LINK2)"
echo ""

echo -e "${GREEN}Step 2: Starting RTU server${NC}"
echo "  Unit ID: 17 (0x11)"
echo "  Baud rate: 9600"
echo "  Device: $LINK1"
echo ""
echo -e "${YELLOW}Press Ctrl+C to stop the server${NC}"
echo ""
echo "----------------------------------------"
echo ""

# Start the RTU server
./build/examples/modbus_rtu_server --device "$LINK1" --baud 9600 --unit 17
