#!/bin/bash
# Simple rebuild script for the example client

cd "/Users/lgili/Documents/01 - Codes/02 - Empresa/02 - Modbus lib"

echo "Rebuilding example_tcp_client_fc03..."
cmake --build build --target example_tcp_client_fc03

echo ""
echo "✓ Build complete!"
echo "Run the integration test with: ./tests/run_integration_test.sh"
