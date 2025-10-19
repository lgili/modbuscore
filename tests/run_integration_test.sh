#!/bin/bash
#
# Script de teste de integração end-to-end.
# Inicia servidor mock, roda cliente, e verifica resultado.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

echo "=== ModbusCore v1.0 - Integration Test ==="
echo ""

# Servidor simples não precisa de pymodbus - usa sockets Python puros

# Verifica se o cliente foi compilado
if [ ! -f "$BUILD_DIR/tests/example_tcp_client_fc03" ]; then
    echo "ERROR: example_tcp_client_fc03 not found"
    echo "Run: cmake --build build"
    exit 1
fi

echo "Step 1: Starting simple Modbus TCP server..."
python3 "$SCRIPT_DIR/simple_tcp_server.py" &
SERVER_PID=$!

# Aguarda servidor iniciar
sleep 2

# Verifica se servidor está rodando
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Failed to start server"
    exit 1
fi

echo "✓ Server running (PID=$SERVER_PID)"
echo ""

echo "Step 2: Running TCP client example..."
echo "========================================"

# Roda cliente e captura saída
if "$BUILD_DIR/tests/example_tcp_client_fc03"; then
    TEST_RESULT="PASS"
else
    TEST_RESULT="FAIL"
fi

echo "========================================"
echo ""

# Cleanup: mata servidor
echo "Step 3: Stopping server..."
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

echo "✓ Server stopped"
echo ""

# Resultado final
if [ "$TEST_RESULT" = "PASS" ]; then
    echo "=========================================="
    echo "   ✓✓✓ INTEGRATION TEST PASSED ✓✓✓"
    echo "=========================================="
    exit 0
else
    echo "=========================================="
    echo "   ✗✗✗ INTEGRATION TEST FAILED ✗✗✗"
    echo "=========================================="
    exit 1
fi
