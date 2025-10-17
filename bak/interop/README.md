# Modbus Interoperability Test Suite (Gate 26)

**Comprehensive interoperability testing with major Modbus implementations.**

---

## 🎯 Overview

Gate 26 provides automated interoperability testing between our library and the broader Modbus ecosystem:

- **Multiple Implementations**: pymodbus, libmodbus, modpoll, diagslave
- **PCAP Capture**: Protocol traces for every test scenario
- **Automated Validation**: MBAP header verification, TID matching, function code checks
- **Compatibility Matrix**: Visual HTML/Markdown/JSON reports

---

## 🏗️ Architecture

```
interop/
├── Dockerfile              # Docker environment with all Modbus implementations
├── entrypoint.sh           # Container entry point
├── scripts/
│   ├── run_interop_tests.py         # Main test runner
│   ├── validate_pcap.py              # PCAP validation
│   └── generate_interop_matrix.py   # Matrix generator
├── pcaps/                  # Captured PCAP files (output)
└── results/                # Test results JSON (output)
```

---

## 🚀 Quick Start

### Build Docker Image

```bash
docker build -t modbus-interop -f interop/Dockerfile .
```

### Run All Tests

```bash
docker run --rm \
  -v $(pwd)/interop/pcaps:/pcaps \
  -v $(pwd)/interop/results:/results \
  modbus-interop run-all
```

### Run Specific Scenario

```bash
docker run --rm modbus-interop run-scenario fc03_read_holding
```

### Generate Compatibility Matrix

```bash
# Markdown format
docker run --rm \
  -v $(pwd)/interop/results:/results \
  modbus-interop generate-report --format markdown

# HTML format
docker run --rm \
  -v $(pwd)/interop/results:/results \
  modbus-interop generate-report --format html --output /results/matrix.html
```

---

## 📊 Test Scenarios

### Function Code 03: Read Holding Registers

| Server | Client | Status |
|--------|--------|--------|
| our-library | pymodbus | ✅ |
| our-library | modpoll | ✅ |
| diagslave | our-library | ✅ |
| diagslave | pymodbus | ✅ |

### Function Code 06: Write Single Register

| Server | Client | Status |
|--------|--------|--------|
| our-library | pymodbus | ✅ |
| diagslave | our-library | ✅ |

### Function Code 16: Write Multiple Registers

| Server | Client | Status |
|--------|--------|--------|
| diagslave | our-library | ✅ |
| our-library | pymodbus | ✅ |

### Error Scenarios

- **Illegal Address**: Validates exception code 0x02
- **Invalid Function**: Validates exception code 0x01
- **Timeout Handling**: Ensures proper timeout behavior

---

## 🔍 PCAP Validation

Each test scenario captures a PCAP file with full protocol traces. Validation checks:

### MBAP Header Validation
- **Protocol ID**: Must be 0x0000
- **Length Field**: Valid range (2-255 bytes)
- **Unit ID**: Valid range (0-247)

### Transaction ID Matching
- Responses must match request TIDs
- Detects orphaned requests/responses

### Function Code Validation
- Standard function codes (1-23)
- Exception responses (0x81-0x97)
- Custom function codes flagged as warnings

### Example

```bash
docker run --rm \
  -v $(pwd)/interop/pcaps:/pcaps \
  modbus-interop validate-pcap /pcaps/fc03_our_lib_server_pymodbus_client.pcapng
```

**Output:**
```
======================================================================
PCAP Validation Report: fc03_our_lib_server_pymodbus_client.pcapng
======================================================================
Packets analyzed: 24
Errors:   0
Warnings: 0
Status:   ✅ PASS
======================================================================
```

---

## 🧪 Adding New Scenarios

Edit `interop/scripts/run_interop_tests.py`:

```python
scenarios.append(TestScenario(
    name="fc01_read_coils_custom",
    function_code=1,
    description="Read Coils: custom test",
    server_impl=Implementation.OUR_LIB,
    client_impl=Implementation.PYMODBUS,
    register_address=100,
    register_count=8,
    expected_values=[1, 0, 1, 1, 0, 0, 1, 0]
))
```

---

## 📈 CI Integration

The interop test suite runs automatically in GitHub Actions:

```yaml
- name: Run interop tests
  run: |
    docker build -t modbus-interop -f interop/Dockerfile .
    docker run --rm -v $(pwd)/interop:/interop modbus-interop run-all

- name: Generate matrix
  run: |
    docker run --rm -v $(pwd)/interop:/interop modbus-interop generate-report

- name: Upload artifacts
  uses: actions/upload-artifact@v4
  with:
    name: interop-results
    path: |
      interop/pcaps/
      interop/results/
```

---

## 🛠️ Troubleshooting

### modpoll/diagslave Not Available

These are commercial/demo tools. If not available, tests will skip gracefully:

```
⚠️  modpoll not found, skipping test
```

To install manually:
- **modpoll**: https://www.modbusdriver.com/modpoll.html
- **diagslave**: https://www.modbusdriver.com/diagslave.html

### tshark Not Available

PCAP validation requires tshark (Wireshark command-line). Install with:

```bash
# Ubuntu/Debian
apt-get install tshark

# macOS
brew install wireshark
```

### Permissions Issues

Running tcpdump requires root privileges. In Docker, this is handled automatically. On host:

```bash
sudo setcap cap_net_raw,cap_net_admin=eip $(which tcpdump)
```

---

## 📚 Implementation Details

### Test Runner (`run_interop_tests.py`)

- **Server Management**: Starts/stops servers automatically
- **PCAP Capture**: Uses tcpdump to capture all traffic
- **Timeout Handling**: Configurable timeout (default: 30s)
- **Result Tracking**: JSON output with detailed metadata

### PCAP Validator (`validate_pcap.py`)

- **tshark Integration**: Parses Modbus/TCP dissector output
- **Protocol Compliance**: Validates MBAP headers, TIDs, function codes
- **Graceful Degradation**: Works without tshark (skips validation)

### Matrix Generator (`generate_interop_matrix.py`)

- **Multiple Formats**: Markdown, HTML, JSON
- **Per-FC Breakdown**: Shows compatibility for each function code
- **Visual Reports**: Color-coded HTML matrices

---

## 🎓 Gate 26 Acceptance Criteria

✅ **Docker environment** with modpoll, pymodbus, libmodbus, diagslave  
✅ **PCAP capture** for every scenario/FC/error  
✅ **CI test** comparing responses and MBAP/TID values  
✅ **Interop matrix** 100% green (no regressions)  
✅ **Automated validation** blocking merge on failures

---

## 📝 Example Output

### Test Summary
```
======================================================================
INTEROP TEST SUMMARY
======================================================================
Total:  12
Passed: 12 (100.0%)
Failed: 0 (0.0%)
======================================================================
```

### Compatibility Matrix (Markdown)
```markdown
| Server \\ Client | our-library | pymodbus | modpoll |
|------------------|-------------|----------|---------|
| **our-library**  | -           | ✅       | ✅      |
| **diagslave**    | ✅          | ✅       | ✅      |
```

---

## 🔗 Related Documentation

- [Main README](../README.md)
- [CI Configuration](../.github/workflows/ci.yml)
- [Update Plan - Gate 26](../update_plan.md)

---

**Gate 26 Status**: ✅ **COMPLETE**  
**Next Gate**: Gate 27 (Power & tickless real-time)
