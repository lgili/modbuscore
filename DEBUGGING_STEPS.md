# Debugging Steps for MBAP Integration

## Current Status

The MBAP implementation is complete, but the integration test is still failing with "No response after 100 iterations". The client is sending 12-byte MBAP frames correctly, but not receiving responses.

## Step 1: Rebuild with Latest Changes

```bash
cd "/Users/lgili/Documents/01 - Codes/02 - Empresa/02 - Modbus lib"
cmake --build build --target example_tcp_client_fc03
```

## Step 2: Test Server with Python Reference Client

First, let's verify the server works correctly with a known-good MBAP client:

```bash
# Terminal 1 - Start server
cd tests
python3 mock_modbus_server.py

# Terminal 2 - Test with Python client
cd tests
python3 test_mbap_manual.py
```

**Expected output from test_mbap_manual.py:**
```
✓ Connected to 127.0.0.1:5502
Request (12 bytes): 00 01 00 00 00 06 01 03 00 00 00 0A
✓ Request sent
Response (25 bytes): 00 01 00 00 00 17 01 03 14 00 00 00 01 00 02 ... 00 09
✓ Test complete
```

**If this works:** The server is fine, issue is in C client
**If this fails:** The server setup has issues

## Step 3: Run C Client with Debug Output

```bash
# Terminal 1 - Server should still be running
# Terminal 2
cd build/tests
./example_tcp_client_fc03
```

Look for the new debug output:
```
Request bytes: 00 01 00 00 00 06 01 03 00 00 00 0A
Engine initial state: state=X, rx_length=Y
```

**Check:**
1. Are the request bytes exactly: `00 01 00 00 00 06 01 03 00 00 00 0A`?
2. What is the engine state after submit_request?
3. Does rx_length ever increase during polling?

## Step 4: Network Traffic Capture

If the above doesn't reveal the issue, capture network traffic:

```bash
# Terminal 1 - Start tcpdump
sudo tcpdump -i lo0 -X port 5502

# Terminal 2 - Start server
cd tests
python3 mock_modbus_server.py

# Terminal 3 - Run client
cd build/tests
./example_tcp_client_fc03
```

**Look for:**
- SYN/SYN-ACK/ACK (connection established)
- Request packet (12 bytes)
- Response packet (should be ~25 bytes for 10 registers)

## Possible Issues and Fixes

### Issue 1: MBAP Encoding Bug

**Symptom:** Request bytes don't match expected format
**Fix:** Check `src/protocol/mbap.c` - the `mbc_mbap_encode()` function

Expected format:
```
00 01        # Transaction ID = 1
00 00        # Protocol ID = 0
00 06        # Length = 6 (unit ID + PDU)
01           # Unit ID
03           # FC03
00 00        # Start addr = 0
00 0A        # Quantity = 10
```

### Issue 2: Engine Not Calling Receive

**Symptom:** `rx_length` never increases, server logs show connection but no request
**Fix:** Check that `mbc_engine_step()` is actually calling `mbc_transport_receive()`

Add temporary debug print in `src/protocol/engine.c` around line 100:
```c
status = mbc_transport_receive(&engine->transport,
                               &engine->rx_buffer[engine->rx_length],
                               space,
                               &io);
printf("DEBUG: receive returned status=%d, processed=%zu\n", status, io.processed);
```

### Issue 3: Wrong Framing Mode

**Symptom:** Engine tries to parse as RTU instead of TCP
**Fix:** Verify engine config has `.framing = MBC_FRAMING_TCP`

Check in `tests/example_tcp_client_fc03.c` around line 99.

### Issue 4: Server Not Responding

**Symptom:** Python test works, C client shows bytes sent, but no response
**Potential causes:**
- Connection closed by server after request
- Server sees malformed request
- Timeout too short

**Fix:** Check server logs for errors, increase client timeout:
```c
.response_timeout_ms = 10000,  // 10 seconds
```

### Issue 5: Response Parsing Bug

**Symptom:** Server sends response (visible in tcpdump), client receives bytes, but PDU not extracted
**Fix:** Check MBAP decode logic in `src/protocol/engine.c` around line 141-168

Add debug print:
```c
if (engine->framing == MBC_FRAMING_TCP) {
    // ... existing code ...
    printf("DEBUG: MBAP decode - trans_id=%u, unit=%u, pdu_len=%zu\n",
           mbap_header.transaction_id, mbap_header.unit_id, pdu_length);
}
```

## Step 5: Compare with Working Python Code

The Python test script (`test_mbap_manual.py`) shows the exact bytes that work. Compare with what the C client sends.

## Quick Test Script

Save this as `quick_test.sh`:
```bash
#!/bin/bash
set -e

echo "=== Quick MBAP Test ==="
echo ""

# Rebuild
echo "1. Rebuilding..."
cmake --build build --target example_tcp_client_fc03 > /dev/null 2>&1
echo "✓ Build complete"

# Start server in background
echo "2. Starting server..."
cd tests
python3 mock_modbus_server.py > /tmp/server.log 2>&1 &
SERVER_PID=$!
cd ..
sleep 2

# Test with Python client
echo "3. Testing with Python client..."
cd tests
python3 test_mbap_manual.py > /tmp/python_test.log 2>&1
PYTHON_RESULT=$?
cd ..

if [ $PYTHON_RESULT -eq 0 ]; then
    echo "✓ Python client works"
else
    echo "✗ Python client failed"
    cat /tmp/python_test.log
fi

# Test with C client
echo "4. Testing with C client..."
./build/tests/example_tcp_client_fc03 > /tmp/c_test.log 2>&1
C_RESULT=$?

if [ $C_RESULT -eq 0 ]; then
    echo "✓ C client works"
else
    echo "✗ C client failed"
    grep -E "(Request bytes|Engine initial|No response)" /tmp/c_test.log
fi

# Cleanup
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

echo ""
echo "Logs saved:"
echo "  Server: /tmp/server.log"
echo "  Python: /tmp/python_test.log"
echo "  C client: /tmp/c_test.log"
```

## Next Action

Please run:

```bash
cd "/Users/lgili/Documents/01 - Codes/02 - Empresa/02 - Modbus lib"

# Rebuild
cmake --build build --target example_tcp_client_fc03

# Test Python client first
cd tests
python3 mock_modbus_server.py &
sleep 2
python3 test_mbap_manual.py
killall python3

# Then test C client
./run_integration_test.sh
```

Share the output and I'll help identify the exact issue!
