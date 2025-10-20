# Scripts

Utility scripts for ModbusCore development.

## Fuzzing Scripts

### `fuzz_in_docker.sh`

Run fuzzing inside Docker container (macOS workaround for LibFuzzer issues).

**Usage:**
```bash
# Quick test (60s, all fuzzers)
./scripts/fuzz_in_docker.sh

# Custom duration
./scripts/fuzz_in_docker.sh 300  # 5 minutes

# Specific fuzzer
./scripts/fuzz_in_docker.sh 60 fuzz_mbap_decoder

# Long campaign
./scripts/fuzz_in_docker.sh 3600  # 1 hour
```

**Requirements:**
- Docker Desktop for Mac, OR
- Colima: `brew install colima docker && colima start`

**What it does:**
1. Builds Ubuntu 24.04 container with Clang/CMake
2. Configures ModbusCore with `ENABLE_FUZZING=ON`
3. Compiles all fuzzing targets
4. Generates seed corpus
5. Runs fuzzing campaign
6. Saves results to `fuzz_results_<timestamp>/`
7. Detects and archives crashes

**Output:**
```
fuzz_results_20250119_210530/
├── fuzz_mbap_decoder.log        # Stats and output
├── fuzz_rtu_crc.log
├── fuzz_pdu_parser.log
└── crashes_<fuzzer>/            # Only if crashes found
    ├── crash-da39a3ee5e6b4b0d
    └── ...
```

### `run_fuzz_campaign.sh`

Native fuzzing campaign script (Linux only).

Runs multiple fuzzers in parallel with real-time monitoring.

**Usage:**
```bash
./scripts/run_fuzz_campaign.sh 3600  # 1 hour
```

## Testing Scripts

### `test_rtu_server.sh`

Launches a test RTU server for integration testing.

**Usage:**
```bash
./scripts/test_rtu_server.sh
```

## Adding New Scripts

When adding scripts:
1. Use `#!/usr/bin/env bash` shebang
2. Add `set -euo pipefail` for safety
3. Make executable: `chmod +x scripts/your_script.sh`
4. Document in this README
5. Add usage examples
