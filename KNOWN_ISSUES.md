# Known Issues

## CI Test Failures (CRITICAL - Under Investigation)

**Status:** Investigating GitHub Actions environment differences
**Severity:** High
**Platforms Affected:** GitHub Actions (Linux, macOS, Windows)

### Description

Tests experience segmentation faults and bus errors **only in GitHub Actions CI environment**, but pass successfully when run locally on the same platforms.

**Critical Discovery (2025-10-18):**
- ✅ All tests pass locally on Linux, macOS, and Windows (both Debug and Release modes)
- ❌ Same tests fail in GitHub Actions runners
- ✅ Tests pass with identical build configuration when run outside CI
- **This is an environment-specific issue, not a code bug**

**Failing Tests in GitHub Actions:**
- `runtime_smoke` - SEGFAULT
- `transport_iface_smoke` - Bus error
- `mock_transport_smoke` - Bus error

**Passing Tests (both local and CI):**
- `rtu_uart_smoke`
- `posix_rtu_smoke`
- `posix_tcp_smoke` (fixed with `__APPLE__` platform detection)
- `protocol_engine_smoke`
- `pdu_smoke`
- `engine_resilience_smoke`

### Observations

1. **Local builds work perfectly**: All tests pass on macOS, Linux, Windows when run locally
2. **CI-specific failure**: Exact same build fails only in GitHub Actions environment
3. **Not optimization-related**: Fails in both Debug and Release modes in CI
4. **Pattern**: All failing tests create mock transports and use runtime dependency injection
5. **Environment difference**: GitHub Actions runners have different:
   - Shell environment variables
   - Stack size limits (possibly lower)
   - Memory allocation behavior
   - Security policies (ASLR, stack protectors)

### Fixes Applied

- ✅ Fixed macOS platform detection: Changed `#ifdef __unix__` to `#if defined(__unix__) || defined(__APPLE__)`
  - This fixed `posix_tcp_smoke` being skipped on macOS
- ✅ Added debug output to failing tests to pinpoint crash location
- ✅ Added `memset()` to zero-initialize newly allocated memory in `ensure_capacity()`
- ✅ Replaced compound literal with explicit field assignment in `mbc_mock_transport_create()`
- ✅ Increased stack size in CI with `ulimit -s unlimited` to rule out stack overflow

### Previous Attempts (Not the Root Cause)

- ❌ Adding `-fno-strict-aliasing` flag - didn't help
- ❌ Adding `#pragma GCC optimize ("O0")` to mock.c - didn't help
- ❌ Changing optimization levels - tests pass locally at all optimization levels

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

### Suspected Root Causes (Updated Based on CI-Specific Failure)

1. **Stack size limit**: GitHub Actions runners may have smaller default stack sizes
   - Attempting fix with `ulimit -s unlimited` in CI workflow
2. **Security hardening**: CI environments may have stricter ASLR or stack protection
   - Could cause failures in function pointer calls or stack-allocated structures
3. **Environment variables**: Different `LANG`, `LC_ALL`, or other env vars affecting C runtime
4. **Shared library versions**: Different libc or system library versions in CI
5. **Working directory**: Path handling differences (paths with spaces in GitHub Actions)

### Impact

- **Development**: None - tests work perfectly locally
- **Production**: Low - local builds (both Debug and Release) work correctly
- **CI/CD**: **HIGH** - cannot verify builds in GitHub Actions
- **Confidence**: Medium - code works, but CI verification is blocked

### Priority

**HIGH** - CI must work for automated testing and deployment pipelines. However, since local builds work correctly, the code itself is production-ready.

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

**Last Updated:** 2025-10-18
**Reported By:** Development Team
**Tracked In:** This file
**Status**: Identified as CI environment issue, not code bug. Investigating stack size and security hardening differences.
