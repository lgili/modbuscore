#!/usr/bin/env bash
#
# run_fuzz_campaign.sh - Run comprehensive fuzzing campaign
#
# Usage:
#   ./scripts/run_fuzz_campaign.sh [duration_seconds]
#
# Examples:
#   ./scripts/run_fuzz_campaign.sh 3600    # 1 hour campaign
#   ./scripts/run_fuzz_campaign.sh 86400   # 24 hour campaign
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

DURATION="${1:-3600}"  # Default 1 hour
BUILD_DIR="${PROJECT_ROOT}/build"
FUZZ_DIR="${BUILD_DIR}/tests/fuzz"
LOG_DIR="${PROJECT_ROOT}/fuzz_logs"

# ANSI colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log() {
    echo -e "${BLUE}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $*"
}

error() {
    echo -e "${RED}[ERROR]${NC} $*" >&2
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $*"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $*"
}

# Check prerequisites
check_prereqs() {
    log "Checking prerequisites..."

    if ! command -v clang &> /dev/null; then
        error "clang not found. Install with: sudo apt-get install clang"
        exit 1
    fi

    if ! command -v cmake &> /dev/null; then
        error "cmake not found. Install with: sudo apt-get install cmake"
        exit 1
    fi

    success "All prerequisites satisfied"
}

# Build fuzzing targets
build_fuzzers() {
    log "Building fuzzing targets..."

    if [ ! -d "${BUILD_DIR}" ]; then
        log "Configuring CMake..."
        cmake -B "${BUILD_DIR}" \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_C_COMPILER=clang \
            -DENABLE_FUZZING=ON \
            -DBUILD_TESTING=OFF
    fi

    log "Compiling fuzzers..."
    cmake --build "${BUILD_DIR}" --parallel --target \
        fuzz_mbap_decoder \
        fuzz_rtu_crc \
        fuzz_pdu_parser \
        generate_corpus

    success "Fuzzers built successfully"
}

# Generate seed corpus
generate_corpus() {
    log "Generating seed corpus..."

    cd "${FUZZ_DIR}"

    if [ ! -f "generate_corpus" ]; then
        error "Corpus generator not found"
        exit 1
    fi

    ./generate_corpus

    local corpus_count=$(find corpus -type f -name "*.bin" 2>/dev/null | wc -l)
    success "Generated ${corpus_count} seed files"

    cd "${PROJECT_ROOT}"
}

# Run single fuzzer in background
run_fuzzer() {
    local target="$1"
    local log_file="${LOG_DIR}/${target}.log"

    log "Starting ${target} (duration: ${DURATION}s)..."

    cd "${FUZZ_DIR}"

    "./${target}" \
        -max_total_time="${DURATION}" \
        -print_final_stats=1 \
        -print_corpus_stats=1 \
        -timeout=10 \
        -rss_limit_mb=2048 \
        -report_slow_units=30 \
        corpus/ \
        > "${log_file}" 2>&1 &

    local pid=$!
    echo "${pid}" > "${LOG_DIR}/${target}.pid"

    log "${target} running (PID: ${pid})"
}

# Monitor fuzzer progress
monitor_fuzzers() {
    log "Monitoring fuzzing progress (Ctrl+C to stop early)..."

    local targets=("fuzz_mbap_decoder" "fuzz_rtu_crc" "fuzz_pdu_parser")
    local start_time=$(date +%s)
    local end_time=$((start_time + DURATION))

    while [ "$(date +%s)" -lt "${end_time}" ]; do
        clear
        echo "=========================================="
        echo "  Fuzzing Campaign Progress"
        echo "=========================================="
        echo ""

        local elapsed=$(($(date +%s) - start_time))
        local remaining=$((end_time - $(date +%s)))

        printf "Elapsed: %02d:%02d:%02d / Target: %02d:%02d:%02d\n" \
            $((elapsed/3600)) $(((elapsed%3600)/60)) $((elapsed%60)) \
            $((DURATION/3600)) $(((DURATION%3600)/60)) $((DURATION%60))
        echo ""

        for target in "${targets[@]}"; do
            local log_file="${LOG_DIR}/${target}.log"
            local pid_file="${LOG_DIR}/${target}.pid"

            if [ -f "${pid_file}" ]; then
                local pid=$(cat "${pid_file}")
                if ps -p "${pid}" > /dev/null 2>&1; then
                    echo "✅ ${target} (PID: ${pid})"

                    # Extract stats from log
                    if [ -f "${log_file}" ]; then
                        local last_stat=$(grep "#" "${log_file}" | tail -n1)
                        if [ -n "${last_stat}" ]; then
                            echo "   ${last_stat}"
                        fi
                    fi
                else
                    echo "❌ ${target} (stopped unexpectedly)"
                fi
            fi
            echo ""
        done

        sleep 5
    done
}

# Check for crashes
check_crashes() {
    log "Checking for crashes..."

    cd "${FUZZ_DIR}"

    local crash_files=$(ls crash-* leak-* timeout-* oom-* 2>/dev/null || true)

    if [ -z "${crash_files}" ]; then
        success "No crashes detected! 🎉"
        return 0
    else
        error "Crashes detected:"
        ls -lh crash-* leak-* timeout-* oom-* 2>/dev/null || true

        # Move crashes to archive
        local crash_archive="${LOG_DIR}/crashes_$(date +'%Y%m%d_%H%M%S')"
        mkdir -p "${crash_archive}"
        mv crash-* leak-* timeout-* oom-* "${crash_archive}/" 2>/dev/null || true

        warning "Crash files archived to: ${crash_archive}"
        return 1
    fi
}

# Generate report
generate_report() {
    log "Generating fuzzing report..."

    local report_file="${LOG_DIR}/report_$(date +'%Y%m%d_%H%M%S').txt"

    {
        echo "=========================================="
        echo "  ModbusCore Fuzzing Campaign Report"
        echo "=========================================="
        echo ""
        echo "Date: $(date)"
        echo "Duration: ${DURATION} seconds"
        echo ""
        echo "=========================================="; echo ""

        for target in fuzz_mbap_decoder fuzz_rtu_crc fuzz_pdu_parser; do
            echo "--- ${target} ---"
            local log_file="${LOG_DIR}/${target}.log"

            if [ -f "${log_file}" ]; then
                # Extract final stats
                grep "DONE" "${log_file}" | tail -n1 || echo "No final stats found"
                echo ""

                # Extract coverage info
                grep "cov:" "${log_file}" | tail -n1 || true
                echo ""
            else
                echo "Log file not found"
                echo ""
            fi
        done

        echo "=========================================="
        echo "Crash Summary:"
        echo "=========================================="

        if check_crashes > /dev/null 2>&1; then
            echo "✅ No crashes detected"
        else
            echo "❌ Crashes found (see ${LOG_DIR}/crashes_*)"
        fi

    } | tee "${report_file}"

    success "Report saved to: ${report_file}"
}

# Cleanup on exit
cleanup() {
    log "Cleaning up..."

    # Kill all fuzzer processes
    for pid_file in "${LOG_DIR}"/*.pid; do
        if [ -f "${pid_file}" ]; then
            local pid=$(cat "${pid_file}")
            if ps -p "${pid}" > /dev/null 2>&1; then
                log "Stopping PID ${pid}..."
                kill "${pid}" 2>/dev/null || true
            fi
            rm -f "${pid_file}"
        fi
    done

    cd "${PROJECT_ROOT}"
}

# Main execution
main() {
    log "Starting fuzzing campaign (duration: ${DURATION}s)"

    mkdir -p "${LOG_DIR}"

    trap cleanup EXIT INT TERM

    check_prereqs
    build_fuzzers
    generate_corpus

    # Run all fuzzers in parallel
    run_fuzzer "fuzz_mbap_decoder"
    run_fuzzer "fuzz_rtu_crc"
    run_fuzzer "fuzz_pdu_parser"

    monitor_fuzzers

    # Wait for all fuzzers to complete
    wait

    check_crashes
    generate_report

    success "Fuzzing campaign complete!"
}

main "$@"
