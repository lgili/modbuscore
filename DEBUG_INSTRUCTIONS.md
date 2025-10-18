# Debugging Integration Test - Next Steps

## What Was Changed

I've added detailed debug logging to help us understand why the integration test is failing:

### 1. Updated `tests/example_tcp_client_fc03.c`
Added verbose status logging in the polling loop (lines 157-160) that will show:
- Status returned by each `mbc_engine_step()` call
- Which iteration we're on
- Will print status for iterations 0-4 and every 10th iteration

### 2. Updated `tests/mock_modbus_server.py`
Added DEBUG logging (lines 34-37) to see:
- All incoming connections
- All received requests
- All sent responses
- Internal pymodbus state

## How to Run the Debug Test

### Step 1: Rebuild the Client

```bash
cd "/Users/lgili/Documents/01 - Codes/02 - Empresa/02 - Modbus lib"
cmake --build build --target example_tcp_client_fc03
```

### Step 2: Run the Integration Test

```bash
./tests/run_integration_test.sh
```

### Step 3: Analyze the Output

The debug output will show us:

**From the client:**
```
Step 5: Polling for response (max 100 iterations)...
  [iter 0] engine_step() returned status=0
  [iter 1] engine_step() returned status=0
  [iter 2] engine_step() returned status=0
  ...
```

**From the server (with DEBUG logging):**
You should see logs like:
- Connection accepted from client
- Request received: `[hex bytes]`
- Response sent: `[hex bytes]`

## What We're Looking For

1. **Is the server receiving the request?**
   - If NO → TCP send is not working
   - If YES → Problem is in response handling

2. **Is the server sending a response?**
   - If NO → Server/request format issue
   - If YES → Problem is in client receive/parsing

3. **What status is `mbc_engine_step()` returning?**
   - 0 (OK) = No error, but no PDU yet
   - 1 (TIMEOUT) = Response timeout
   - Other = Some error occurred

## Likely Root Causes

Based on the symptoms (connection works, request submits, but no response), the issue is probably:

1. **TCP send not actually transmitting** - The request buffer may not be sent over the socket
2. **Request format wrong** - Server doesn't recognize the request
3. **Response not being received** - Socket receive not working
4. **Engine state machine stuck** - FSM not transitioning to receive state

## Manual Test Alternative

If the integration test still fails, try running manually in two terminals:

**Terminal 1:**
```bash
cd tests
python3 mock_modbus_server.py
```

**Terminal 2:**
```bash
cd build/tests
./example_tcp_client_fc03
```

Watch the server terminal for incoming connections and requests.

## Next Actions After Running

Once you run the test with debug output, share the full output with me so we can:
1. See what status codes are being returned
2. Check if the server is receiving anything
3. Determine the exact point of failure
4. Fix the root cause
