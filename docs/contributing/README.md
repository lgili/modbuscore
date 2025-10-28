# Contributing to ModbusCore

Thank you for your interest in contributing! This guide will help you get started.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Submitting Changes](#submitting-changes)
- [Release Process](#release-process)

## Code of Conduct

We are committed to providing a welcoming and inclusive environment:

- ✅ **Be respectful** - Value diverse opinions and experiences
- ✅ **Be constructive** - Provide helpful feedback
- ✅ **Be collaborative** - Work together towards common goals
- ❌ **No harassment** - Zero tolerance for abusive behavior

## Getting Started

### Prerequisites

- **C Compiler**: GCC 9+, Clang 12+, or MSVC 2019+
- **CMake**: 3.20 or higher
- **Git**: For version control
- **clang-format**: For code formatting
- **clang-tidy**: For static analysis (optional)

### Fork and Clone

```bash
# Fork on GitHub, then clone your fork
git clone https://github.com/YOUR_USERNAME/modbuscore.git
cd modbuscore

# Add upstream remote
git remote add upstream https://github.com/lgili/modbuscore.git
```

### Build and Test

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON

# Build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

## Development Setup

### IDE Configuration

#### VS Code

Install recommended extensions:
- **C/C++** (ms-vscode.cpptools)
- **CMake Tools** (ms-vscode.cmake-tools)
- **clang-format** (xaver.clang-format)

`.vscode/settings.json`:
```json
{
    "cmake.configureOnOpen": true,
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "editor.formatOnSave": true,
    "C_Cpp.clang_format_style": "file"
}
```

#### CLion

1. Open project via **File → Open → CMakeLists.txt**
2. Configure CMake profile: **Debug** with `-DBUILD_TESTING=ON`
3. Set code style: **Settings → Editor → Code Style → C/C++** → Import `.clang-format`

### Code Formatting

We use **clang-format** with LLVM style:

```bash
# Format all source files
find modbus -name '*.c' -o -name '*.h' | xargs clang-format -i

# Check formatting (CI)
find modbus -name '*.c' -o -name '*.h' | xargs clang-format --dry-run --Werror
```

`.clang-format`:
```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
AllowShortFunctionsOnASingleLine: Empty
```

### Static Analysis

Run clang-tidy:

```bash
cmake -B build -DCMAKE_C_CLANG_TIDY="clang-tidy;-warnings-as-errors=*"
cmake --build build
```

## Coding Standards

### C17 Standard

- ✅ Use C17 features
- ❌ No C++ code
- ❌ No compiler-specific extensions

### Naming Conventions

| Type | Convention | Example |
|------|-----------|---------|
| **Functions** | `mbc_module_action()` | `mbc_pdu_build_request()` |
| **Types** | `mbc_name_t` | `mbc_pdu_t` |
| **Enums** | `MBC_ENUM_VALUE` | `MBC_STATUS_OK` |
| **Macros** | `MBC_MACRO_NAME` | `MBC_PDU_MAX_LENGTH` |
| **Local vars** | `snake_case` | `uint8_t tx_buffer[260];` |

### File Organization

```c
// header.h
#ifndef MODBUSCORE_MODULE_HEADER_H
#define MODBUSCORE_MODULE_HEADER_H

#include <stdint.h>
#include <stddef.h>

// Public types
typedef struct { ... } mbc_type_t;

// Public functions
mbc_status_t mbc_function(mbc_type_t *obj);

#endif
```

```c
// source.c
#include "modbuscore/module/header.h"
#include <string.h>

// Static helpers
static void helper_function(void) {
    // Implementation
}

// Public API
mbc_status_t mbc_function(mbc_type_t *obj) {
    // Implementation
}
```

### Error Handling

**Always return status codes:**

```c
// ✅ Good
mbc_status_t mbc_do_something(mbc_obj_t *obj) {
    if (obj == NULL) {
        return MBC_STATUS_INVALID_PARAMETER;
    }
    // ...
    return MBC_STATUS_OK;
}

// ❌ Bad: void return
void mbc_do_something(mbc_obj_t *obj) {
    // No error reporting!
}
```

**Validate parameters:**

```c
mbc_status_t mbc_function(const uint8_t *data, size_t len) {
    // Validate all inputs
    if (data == NULL) return MBC_STATUS_INVALID_PARAMETER;
    if (len == 0) return MBC_STATUS_INVALID_PARAMETER;
    
    // Implementation
    return MBC_STATUS_OK;
}
```

### Memory Management

**No dynamic allocation:**

```c
// ✅ Good: Stack allocation
uint8_t buffer[260];

// ❌ Bad: Heap allocation
uint8_t *buffer = malloc(260);
free(buffer);
```

**Fixed-size structures:**

```c
// ✅ Good: Fixed size
typedef struct {
    uint8_t data[252];
    size_t length;
} mbc_pdu_t;

// ❌ Bad: Variable size
typedef struct {
    uint8_t *data;  // Requires malloc
    size_t length;
} mbc_pdu_t;
```

### Documentation

Use Doxygen-style comments:

```c
/**
 * @brief Build a read holding registers request PDU.
 * 
 * Creates a Modbus function 0x03 (Read Holding Registers) PDU with the
 * specified slave address, starting address, and quantity of registers.
 * 
 * @param pdu          Output PDU structure to populate
 * @param slave_addr   Modbus slave address (1-247)
 * @param start_addr   Starting register address (0-65535)
 * @param count        Number of registers to read (1-125)
 * 
 * @return MBC_STATUS_OK on success
 * @return MBC_STATUS_INVALID_PARAMETER if parameters are invalid
 * 
 * @note Maximum count is 125 to comply with Modbus specification
 * 
 * @code
 * mbc_pdu_t pdu = {0};
 * mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
 * if (status == MBC_STATUS_OK) {
 *     // Use pdu...
 * }
 * @endcode
 */
mbc_status_t mbc_pdu_build_read_holding_request(
    mbc_pdu_t *pdu,
    uint8_t slave_addr,
    uint16_t start_addr,
    uint16_t count
);
```

## Testing

### Unit Tests

Create tests in `tests/`:

```c
// tests/test_my_feature.c
#include "modbuscore/protocol/pdu.h"
#include "test_common.h"
#include <assert.h>

void test_build_request(void) {
    mbc_pdu_t pdu = {0};
    mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    
    assert(status == MBC_STATUS_OK);
    assert(pdu.function == 0x03);
    assert(pdu.data_length == 4);
}

int main(void) {
    test_build_request();
    printf("✓ All tests passed\n");
    return 0;
}
```

Add to `tests/CMakeLists.txt`:

```cmake
add_executable(test_my_feature test_my_feature.c)
target_link_libraries(test_my_feature modbuscore)
add_test(NAME test_my_feature COMMAND test_my_feature)
```

### Integration Tests

Test with real Modbus devices or simulators:

```bash
# Start simulator
./build/examples/tcp_server_demo &

# Run integration test
./build/tests/integration_tcp_test
```

### Coverage

Generate coverage reports:

```bash
cmake -B build-coverage \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="--coverage" \
    -DBUILD_TESTING=ON

cmake --build build-coverage
ctest --test-dir build-coverage

# Generate report
lcov --capture --directory build-coverage --output-file coverage.info
genhtml coverage.info --output-directory build-coverage/coverage
```

View: `open build-coverage/coverage/index.html`

## Submitting Changes

### Branch Naming

| Type | Prefix | Example |
|------|--------|---------|
| **Feature** | `feature/` | `feature/add-diagnostics` |
| **Bugfix** | `fix/` | `fix/crc-calculation` |
| **Docs** | `docs/` | `docs/update-readme` |
| **Test** | `test/` | `test/add-rtu-tests` |

### Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation only
- `test`: Adding tests
- `refactor`: Code refactoring
- `perf`: Performance improvement
- `ci`: CI/CD changes

**Example:**
```
feat(protocol): add write multiple coils support

Implement mbc_pdu_build_write_multiple_coils_request() to support
Modbus function 0x0F (Write Multiple Coils).

- Add PDU building function
- Add unit tests
- Update documentation

Closes #42
```

### Pull Request Process

1. **Update from upstream:**
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

2. **Run all checks:**
   ```bash
   # Format
   find modbus -name '*.c' -o -name '*.h' | xargs clang-format -i
   
   # Build
   cmake -B build -DBUILD_TESTING=ON
   cmake --build build
   
   # Test
   ctest --test-dir build --output-on-failure
   ```

3. **Push to your fork:**
   ```bash
   git push origin feature/my-feature
   ```

4. **Create Pull Request** on GitHub:
   - Clear title and description
   - Reference related issues
   - Include test results
   - Add screenshots if UI changes

5. **Address review feedback:**
   ```bash
   # Make changes
   git add .
   git commit -m "fix: address review feedback"
   git push origin feature/my-feature
   ```

### PR Checklist

- [ ] Code follows style guide
- [ ] All tests pass
- [ ] New tests added for new features
- [ ] Documentation updated
- [ ] CHANGELOG.md updated
- [ ] No compiler warnings
- [ ] Commit messages follow conventions

## Release Process

### Version Numbering

We use [Semantic Versioning](https://semver.org/):

- **Major** (X.0.0): Breaking API changes
- **Minor** (1.X.0): New features, backward compatible
- **Patch** (1.0.X): Bug fixes, backward compatible

### Release Steps

1. **Update version:**
   ```bash
   # Edit CMakeLists.txt
   project(ModbusCore VERSION 1.1.0)
   
   # Edit modbus/version.h.in
   #define MBC_VERSION_MAJOR 1
   #define MBC_VERSION_MINOR 1
   #define MBC_VERSION_PATCH 0
   ```

2. **Update CHANGELOG.md:**
   ```markdown
   ## [1.1.0] - 2025-10-21
   
   ### Added
   - New feature X
   
   ### Fixed
   - Bug Y
   ```

3. **Create release tag:**
   ```bash
   git tag -a v1.1.0 -m "Release v1.1.0"
   git push upstream v1.1.0
   ```

4. **GitHub Release:**
   - Create release on GitHub
   - Attach release notes
   - Upload artifacts (if any)

5. **Update package managers:**
   - Conan: Update `conanfile.py`
   - vcpkg: Update `vcpkg.json`
   - Notify downstream projects

## Community

- **Issues**: [GitHub Issues](https://github.com/lgili/modbuscore/issues)
- **Discussions**: [GitHub Discussions](https://github.com/lgili/modbuscore/discussions)
- **Email**: modbuscore@example.com

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

Thank you for contributing to ModbusCore! 🎉
