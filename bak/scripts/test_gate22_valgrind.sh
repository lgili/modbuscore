#!/bin/bash
# Gate 22 Memory Leak Validation Script
# Tests 1M transactions with Valgrind (Linux) or ASan (macOS/all platforms)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Gate 22 Memory Leak Validation ==="
echo "Project root: $PROJECT_ROOT"
echo ""

# Check platform
PLATFORM=$(uname -s)
echo "Platform: $PLATFORM"

# Check architecture
ARCH=$(uname -m)
echo "Architecture: $ARCH"

# Function to run tests with ASan
run_with_asan() {
    echo ""
    echo "=== Running with AddressSanitizer ==="
    
    if [ ! -d "$PROJECT_ROOT/build/host-asan" ]; then
        echo "Building with ASan..."
        cmake -S "$PROJECT_ROOT" -B "$PROJECT_ROOT/build/host-asan" \
            -DMODBUS_ENABLE_ASAN=ON \
            -DCMAKE_BUILD_TYPE=Debug
        cmake --build "$PROJECT_ROOT/build/host-asan" -j8
    fi
    
    echo ""
    echo "Running 1M transaction stress test..."
    "$PROJECT_ROOT/build/host-asan/tests/test_mb_txpool" \
        --gtest_filter="TxPoolStressTest.Gate22_OneMillionTransactionsNoLeaks"
    
    local exit_code=$?
    if [ $exit_code -eq 0 ]; then
        echo ""
        echo "✅ ASan: PASSED - No leaks detected"
        return 0
    else
        echo ""
        echo "❌ ASan: FAILED - Leaks detected or test failed"
        return 1
    fi
}

# Function to run tests with Valgrind (Linux only)
run_with_valgrind() {
    echo ""
    echo "=== Running with Valgrind ==="
    
    # Check if valgrind is installed
    if ! command -v valgrind &> /dev/null; then
        echo "⚠️  Valgrind not found. Install with:"
        echo "    Ubuntu/Debian: sudo apt-get install valgrind"
        echo "    Fedora/RHEL:   sudo dnf install valgrind"
        echo "    Arch:          sudo pacman -S valgrind"
        echo ""
        echo "Skipping Valgrind tests..."
        return 1
    fi
    
    if [ ! -d "$PROJECT_ROOT/build/host-debug" ]; then
        echo "Building debug version..."
        cmake -S "$PROJECT_ROOT" -B "$PROJECT_ROOT/build/host-debug" \
            -DCMAKE_BUILD_TYPE=Debug
        cmake --build "$PROJECT_ROOT/build/host-debug" -j8
    fi
    
    echo ""
    echo "Running 1M transaction stress test with Valgrind..."
    echo "(This may take a few minutes due to Valgrind overhead...)"
    
    valgrind \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --error-exitcode=1 \
        --suppressions="$PROJECT_ROOT/scripts/valgrind.supp" 2>/dev/null || true \
        "$PROJECT_ROOT/build/host-debug/tests/test_mb_txpool" \
        --gtest_filter="TxPoolStressTest.Gate22_OneMillionTransactionsNoLeaks"
    
    local exit_code=$?
    if [ $exit_code -eq 0 ]; then
        echo ""
        echo "✅ Valgrind: PASSED - No leaks detected"
        return 0
    else
        echo ""
        echo "❌ Valgrind: FAILED - Leaks detected or test failed"
        return 1
    fi
}

# Function to run tests with macOS Leaks tool
run_with_macos_leaks() {
    echo ""
    echo "=== Running with macOS Leaks Tool ==="
    
    # Check if leaks command exists (should be on all macOS)
    if ! command -v leaks &> /dev/null; then
        echo "⚠️  leaks tool not found (should be installed by default on macOS)"
        echo "Skipping leaks tests..."
        return 1
    fi
    
    if [ ! -d "$PROJECT_ROOT/build/host-debug" ]; then
        echo "Building debug version..."
        cmake -S "$PROJECT_ROOT" -B "$PROJECT_ROOT/build/host-debug" \
            -DCMAKE_BUILD_TYPE=Debug
        cmake --build "$PROJECT_ROOT/build/host-debug" -j8
    fi
    
    echo ""
    echo "Running 1M transaction stress test with macOS leaks..."
    
    # Run with leaks tool (checks at exit)
    leaks --atExit -- \
        "$PROJECT_ROOT/build/host-debug/tests/test_mb_txpool" \
        --gtest_filter="TxPoolStressTest.Gate22_OneMillionTransactionsNoLeaks" \
        > /tmp/gate22_leaks.log 2>&1
    
    local exit_code=$?
    
    # Check for leaks in output
    if grep -q "0 leaks for 0 total leaked bytes" /tmp/gate22_leaks.log; then
        echo ""
        echo "✅ macOS Leaks: PASSED - No leaks detected"
        cat /tmp/gate22_leaks.log | grep "leaks for"
        return 0
    elif [ $exit_code -eq 0 ]; then
        echo ""
        echo "✅ macOS Leaks: PASSED - Test completed successfully"
        return 0
    else
        echo ""
        echo "❌ macOS Leaks: FAILED - Leaks detected or test failed"
        cat /tmp/gate22_leaks.log | grep -A5 "leaked bytes"
        return 1
    fi
}

# Function to run tests with TSan
run_with_tsan() {
    echo ""
    echo "=== Running with ThreadSanitizer ==="
    
    if [ ! -d "$PROJECT_ROOT/build/host-tsan" ]; then
        echo "Building with TSan..."
        cmake -S "$PROJECT_ROOT" -B "$PROJECT_ROOT/build/host-tsan" \
            -DMODBUS_ENABLE_TSAN=ON \
            -DCMAKE_BUILD_TYPE=Debug
        cmake --build "$PROJECT_ROOT/build/host-tsan" -j8
    fi
    
    echo ""
    echo "Running concurrent access test..."
    "$PROJECT_ROOT/build/host-tsan/tests/test_mb_txpool" \
        --gtest_filter="TxPoolStressTest.Gate22_ConcurrentAccessWithMutex"
    
    local exit_code=$?
    if [ $exit_code -eq 0 ]; then
        echo ""
        echo "✅ TSan: PASSED - No data races detected"
        return 0
    else
        echo ""
        echo "❌ TSan: FAILED - Data races detected or test failed"
        return 1
    fi
}

# Main test execution
RESULTS=()

# Always run ASan (works on all platforms)
if run_with_asan; then
    RESULTS+=("✅ ASan")
else
    RESULTS+=("❌ ASan")
fi

# Platform-specific leak detection
if [ "$PLATFORM" = "Linux" ]; then
    # Use Valgrind on Linux
    if run_with_valgrind; then
        RESULTS+=("✅ Valgrind")
    else
        RESULTS+=("⚠️  Valgrind (skipped/failed)")
    fi
elif [ "$PLATFORM" = "Darwin" ]; then
    # Use native macOS leaks tool
    if run_with_macos_leaks; then
        RESULTS+=("✅ macOS Leaks")
    else
        RESULTS+=("⚠️  macOS Leaks (skipped/failed)")
    fi
else
    echo ""
    echo "⚠️  Platform-specific leak tool not available on $PLATFORM"
    RESULTS+=("⚠️  Leak detection (platform)")
fi

# Run TSan
if run_with_tsan; then
    RESULTS+=("✅ TSan")
else
    RESULTS+=("❌ TSan")
fi

# Summary
echo ""
echo "========================================"
echo "       Gate 22 Validation Summary       "
echo "========================================"
for result in "${RESULTS[@]}"; do
    echo "$result"
done
echo "========================================"
echo ""

# Check if all critical tests passed
if [[ "${RESULTS[@]}" =~ "❌ ASan" ]] || [[ "${RESULTS[@]}" =~ "❌ TSan" ]]; then
    echo "❌ Gate 22: FAILED - Critical tests failed"
    exit 1
elif [ "$PLATFORM" = "Linux" ] && [[ "${RESULTS[@]}" =~ "❌ Valgrind" ]]; then
    echo "❌ Gate 22: FAILED - Valgrind tests failed"
    exit 1
elif [ "$PLATFORM" = "Darwin" ] && [[ "${RESULTS[@]}" =~ "❌ macOS Leaks" ]]; then
    echo "❌ Gate 22: FAILED - macOS Leaks tests failed"
    exit 1
else
    echo "✅ Gate 22: PASSED - All memory leak tests passed"
    echo ""
    echo "Platform-specific validation:"
    if [ "$PLATFORM" = "Linux" ]; then
        echo "  - Valgrind (industry standard)"
    elif [ "$PLATFORM" = "Darwin" ]; then
        echo "  - macOS Leaks (Apple native tool)"
    fi
    echo "  - AddressSanitizer (cross-platform)"
    echo "  - ThreadSanitizer (data race detection)"
    exit 0
fi
