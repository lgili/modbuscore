# Known Issues

## Release Mode Test Failures (CRITICAL - Needs Investigation)

**Status:** Workaround in place (using Debug builds)
**Severity:** High
**Platforms Affected:** Linux, macOS, Windows

### Description

Tests experience segmentation faults and bus errors when compiled in Release mode (`-O2` or `-O3` optimization), but pass successfully in Debug mode (`-O0` or `-Og`).

**Failing Tests in Release Mode:**
- `runtime_smoke` - SEGFAULT
- `transport_iface_smoke` - Bus error
- `mock_transport_smoke` - Bus error
- `engine_resilience_smoke` - SIGTRAP
- `posix_tcp_smoke` - SEGFAULT (Linux only)

**Passing Tests:**
- `rtu_uart_smoke`
- `posix_rtu_smoke`
- `posix_tcp_smoke` (macOS)
- `protocol_engine_smoke`
- `pdu_smoke`

### Observations

1. **Debug mode works perfectly**: All tests pass with `-O0` optimization
2. **AddressSanitizer works**: Tests pass when compiled with `-fsanitize=address`
3. **Isolated tests work**: Minimal reproduction tests pass even with `-O2`
4. **CTest-specific**: Tests fail via `ctest` but sometimes pass when run directly
5. **Pattern**: All failing tests use the mock transport or runtime components

### Attempted Fixes (Did Not Resolve)

- ✅ Added `memset()` to zero-initialize newly allocated memory in `ensure_capacity()`
- ✅ Replaced compound literal with explicit field assignment in `mbc_mock_transport_create()`
- ❌ Added `-fno-strict-aliasing` flag
- ❌ Added `#pragma GCC optimize ("O0")` to mock.c
- ❌ Checked for uninitialized variables
- ❌ Verified no dangling pointers

### Current Workaround

The CI/CD pipeline has been configured to use Debug builds (`-DCMAKE_BUILD_TYPE=Debug`) to ensure test stability.

**File:** `.github/workflows/ci.yml`

```yaml
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..
```

### Next Steps for Investigation

1. **Use Valgrind** (Linux) to check for memory issues:
   ```bash
   valgrind --leak-check=full --track-origins=yes ./tests/runtime_smoke
   ```

2. **Disassemble** the failing code to see what optimizations are being applied:
   ```bash
   objdump -d -S tests/runtime_smoke > runtime_smoke_release.asm
   ```

3. **Compare Debug vs Release** assembly to identify problematic optimizations

4. **Check for strict aliasing violations** - use `-Wstrict-aliasing=3`

5. **Test with different optimization levels**:
   - `-O1`: Does it fail?
   - `-Os`: Does it fail?
   - `-O2 -fno-inline`: Does it fail?

6. **Verify struct padding and alignment** - check if there are any assumptions about struct layout

7. **Review all pointer arithmetic** in mock.c, especially in:
   - `ensure_capacity()` - realloc and memset logic
   - `list_insert_frame()` - memmove logic
   - `list_remove_at()` - memmove logic

### Suspected Root Causes

1. **Undefined Behavior**: Some operation is invoking UB that gets exposed by optimization
2. **Stack corruption**: Local variables being corrupted by buffer overruns
3. **Use-after-free**: Pointers being used after memory is freed (not caught by ASAN?)
4. **Alignment issues**: Unaligned access being optimized incorrectly
5. **Volatile missing**: Variables being cached in registers when they shouldn't be

### Impact

- **Development**: Minimal - tests work in Debug mode
- **Production**: **HIGH** - production builds should use Release mode
- **CI/CD**: Mitigated - using Debug builds for now

### Priority

**HIGH** - This must be fixed before production release. Debug builds are acceptable for development but not for production deployments.

---

## Workaround for Local Development

If you need to test Release builds locally:

```bash
# Force Debug mode
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON ..

# Or use AddressSanitizer (slower but catches more issues)
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-fsanitize=address -g" -DBUILD_TESTING=ON ..
```

---

**Last Updated:** 2025-10-17
**Reported By:** Development Team
**Tracked In:** This file
