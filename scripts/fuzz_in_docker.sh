#!/usr/bin/env bash
#
# fuzz_in_docker.sh - Run fuzzing inside Docker container (macOS workaround)
#
# This script builds and runs the ModbusCore fuzzers inside an Ubuntu container,
# bypassing the LibFuzzer linkage issues on macOS ARM64.
#
# Usage:
#   ./scripts/fuzz_in_docker.sh [duration] [fuzzer]
#
# Examples:
#   ./scripts/fuzz_in_docker.sh                    # Run all fuzzers for 60s
#   ./scripts/fuzz_in_docker.sh 300                # Run all fuzzers for 5min
#   ./scripts/fuzz_in_docker.sh 60 fuzz_mbap_decoder  # Run specific fuzzer
#

set -euo pipefail

# Configuration
DURATION="${1:-60}"
SPECIFIC_FUZZER="${2:-}"
DOCKER_IMAGE="ubuntu:24.04"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# ANSI colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log() {
    echo -e "${BLUE}[$(date +'%H:%M:%S')]${NC} $*"
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

# Check if Docker is available
check_docker() {
    if ! command -v docker &> /dev/null; then
        error "Docker is not installed or not in PATH"
        echo ""
        echo "Install Docker Desktop for Mac:"
        echo "  https://docs.docker.com/desktop/install/mac-install/"
        echo ""
        echo "Or use Colima (lightweight alternative):"
        echo "  brew install colima docker"
        echo "  colima start"
        exit 1
    fi

    if ! docker ps &> /dev/null; then
        error "Docker daemon is not running"
        echo ""
        echo "Start Docker Desktop or run:"
        echo "  colima start"
        exit 1
    fi

    success "Docker is available"
}

# Build Docker image with dependencies
build_image() {
    log "Building Docker image with fuzzing dependencies..."

    docker build -t modbuscore-fuzzer - <<'DOCKERFILE'
FROM ubuntu:24.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    clang \
    cmake \
    python3 \
    build-essential \
    git \
    && rm -rf /var/lib/apt/lists/*

# Verify clang version
RUN clang --version

WORKDIR /work
DOCKERFILE

    success "Docker image built"
}

# Run fuzzing in container
run_fuzzing() {
    log "Starting fuzzing in Docker container..."
    log "Duration: ${DURATION}s"

    if [ -n "${SPECIFIC_FUZZER}" ]; then
        log "Running specific fuzzer: ${SPECIFIC_FUZZER}"
    else
        log "Running all fuzzers"
    fi

    # Create output directory for results
    local output_dir="${PROJECT_ROOT}/fuzz_results_$(date +'%Y%m%d_%H%M%S')"
    mkdir -p "${output_dir}"

    docker run --rm -it \
        -v "${PROJECT_ROOT}:/work" \
        -w /work \
        modbuscore-fuzzer \
        bash -c "
            set -e

            echo '====================================='
            echo '  ModbusCore Fuzzing in Docker'
            echo '====================================='
            echo ''
            echo 'System info:'
            uname -a
            clang --version | head -n1
            echo ''

            # Configure CMake with fuzzing
            echo '>>> Configuring CMake...'
            cmake -B build \
                -DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_C_COMPILER=clang \
                -DENABLE_FUZZING=ON \
                -DBUILD_TESTING=OFF

            # Build fuzzers
            echo ''
            echo '>>> Building fuzzers...'
            cmake --build build --parallel

            # Generate corpus
            echo ''
            echo '>>> Generating seed corpus...'
            cd build/tests/fuzz
            ./generate_corpus
            mkdir -p corpus
            mv *.bin corpus/ 2>/dev/null || true
            mkdir -p /work/tests/fuzz/corpus
            cp corpus/*.bin /work/tests/fuzz/corpus/ 2>/dev/null || true
            ls -lh corpus/

            echo ''
            echo '====================================='
            echo '  Starting Fuzzing Campaign'
            echo '====================================='
            echo ''

            # Function to run a single fuzzer
            run_fuzzer() {
                local fuzzer=\$1
                local log_file=\"/work/fuzz_results/\${fuzzer}.log\"

                echo \">>> Running \${fuzzer}...\"

                timeout ${DURATION}s ./\${fuzzer} \
                    -max_total_time=${DURATION} \
                    -print_final_stats=1 \
                    -print_corpus_stats=1 \
                    -timeout=10 \
                    -rss_limit_mb=2048 \
                    corpus/ \
                    > \"\${log_file}\" 2>&1 || true

                echo \"\"
                echo \">>> \${fuzzer} finished\"

                # Show last stats
                if [ -f \"\${log_file}\" ]; then
                    echo \"Final stats:\"
                    grep 'DONE' \"\${log_file}\" | tail -n1 || echo \"No final stats\"
                fi

                # Check for crashes
                if ls crash-* leak-* timeout-* oom-* 2>/dev/null; then
                    echo \"❌ CRASHES FOUND for \${fuzzer}\"
                    mkdir -p \"/work/fuzz_results/crashes_\${fuzzer}\"
                    mv crash-* leak-* timeout-* oom-* \"/work/fuzz_results/crashes_\${fuzzer}/\" 2>/dev/null || true
                    return 1
                else
                    echo \"✅ No crashes detected\"
                    return 0
                fi
            }

            # Create results directory
            mkdir -p /work/fuzz_results

            # Run specific fuzzer or all
            if [ -n '${SPECIFIC_FUZZER}' ]; then
                run_fuzzer '${SPECIFIC_FUZZER}'
            else
                # Run all fuzzers sequentially
                FAILED=0

                run_fuzzer 'fuzz_mbap_decoder' || FAILED=1
                echo ''
                run_fuzzer 'fuzz_rtu_crc' || FAILED=1
                echo ''
                run_fuzzer 'fuzz_pdu_parser' || FAILED=1

                if [ \$FAILED -eq 1 ]; then
                    echo ''
                    echo '❌ Some fuzzers found crashes'
                    exit 1
                fi
            fi

            echo ''
            echo '====================================='
            echo '  Fuzzing Campaign Complete'
            echo '====================================='
            echo ''
            echo 'Results saved to: fuzz_results/'
        "

    local exit_code=$?

    # Move results to timestamped directory
    if [ -d "${PROJECT_ROOT}/fuzz_results" ]; then
        mv "${PROJECT_ROOT}/fuzz_results" "${output_dir}"
        success "Results saved to: ${output_dir}"

        # Show summary
        echo ""
        log "Summary:"
        for log_file in "${output_dir}"/*.log; do
            if [ -f "${log_file}" ]; then
                local fuzzer=$(basename "${log_file}" .log)
                echo "  ${fuzzer}:"
                grep "DONE" "${log_file}" | tail -n1 | sed 's/^/    /' || echo "    No stats available"
            fi
        done

        # Check for crashes
        if ls "${output_dir}"/crashes_* 2>/dev/null | grep -q .; then
            echo ""
            warning "Crashes found! Check: ${output_dir}/crashes_*"
        else
            echo ""
            success "No crashes detected! 🎉"
        fi
    fi

    return ${exit_code}
}

# Main execution
main() {
    log "ModbusCore Docker Fuzzing Script"
    log "================================="
    echo ""

    check_docker

    # Check if image needs to be built
    if ! docker images modbuscore-fuzzer --format "{{.Repository}}" | grep -q "modbuscore-fuzzer"; then
        build_image
    else
        log "Using cached Docker image (run 'docker rmi modbuscore-fuzzer' to rebuild)"
    fi

    echo ""
    run_fuzzing

    echo ""
    success "Fuzzing complete!"
    echo ""
    echo "Next steps:"
    echo "  - Review results in the fuzz_results_*/ directory"
    echo "  - If crashes found, reproduce with: ./build/tests/fuzz/<fuzzer> crash-<hash>"
    echo "  - Run longer campaigns: $0 3600  # 1 hour"
}

main "$@"
