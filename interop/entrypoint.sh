#!/bin/bash
# Entrypoint for Modbus interop Docker container
# Gate 26: Interop rig & golden pcaps

set -e

show_help() {
    cat <<EOF
Modbus Interoperability Test Environment (Gate 26)

Usage:
    docker run --rm modbus-interop [COMMAND] [OPTIONS]

Commands:
    test-simple             Run simple baseline test (pymodbus only)
    test-expanded           Run expanded test suite (all implementations + all FCs)
    run-all                 Run all interop scenarios and generate reports
    run-scenario <name>     Run a specific test scenario
    list-scenarios          List available test scenarios
    validate-pcap <file>    Validate a captured PCAP file
    generate-report         Generate interop compatibility matrix
    --help                  Show this help message

Examples:
    # Run simple baseline test (recommended first)
    docker run --rm modbus-interop test-simple

    # Run expanded test suite (all function codes + implementations)
    docker run --rm -v \$(pwd)/interop/results:/results modbus-interop test-expanded

    # Run all interop tests
    docker run --rm -v \$(pwd)/interop/pcaps:/pcaps modbus-interop run-all

    # Run specific scenario
    docker run --rm modbus-interop run-scenario fc03_read_holding

    # Generate report only
    docker run --rm -v \$(pwd)/interop/results:/results modbus-interop generate-report

Environment Variables:
    CAPTURE_PCAP=1         Enable PCAP capture (default: 1)
    VERBOSE=1              Enable verbose logging
    TIMEOUT=30             Test timeout in seconds (default: 30)

EOF
}

case "${1:-}" in
    test-simple)
        exec python3 /interop-scripts/simple_interop_test.py
        ;;
    test-expanded)
        exec python3 /interop-scripts/expanded_interop_test.py
        ;;
    run-all)
        shift
        exec python3 /interop-scripts/run_interop_tests.py --all "$@"
        ;;
    run-scenario)
        shift
        exec python3 /interop-scripts/run_interop_tests.py --scenario "$@"
        ;;
    list-scenarios)
        exec python3 /interop-scripts/run_interop_tests.py --list
        ;;
    validate-pcap)
        shift
        exec python3 /interop-scripts/validate_pcap.py "$@"
        ;;
    generate-report)
        exec python3 /interop-scripts/generate_interop_matrix.py
        ;;
    --help|-h|help|"")
        show_help
        exit 0
        ;;
    *)
        echo "Unknown command: $1"
        echo "Run with --help for usage information"
        exit 1
        ;;
esac
