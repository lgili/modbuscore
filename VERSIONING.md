# Versioning Policy

ModbusCore follows [Semantic Versioning 2.0.0](https://semver.org/).

## Version Format

```
MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]
```

- **MAJOR**: Incompatible API changes
- **MINOR**: New functionality (backward-compatible)
- **PATCH**: Bug fixes (backward-compatible)
- **PRERELEASE**: Optional pre-release identifier (alpha, beta, rc1, etc.)
- **BUILD**: Optional build metadata

### Examples

- `1.0.0` - First stable release
- `1.1.0` - New features added
- `1.1.1` - Bug fix
- `2.0.0` - Breaking API changes
- `1.2.0-beta.1` - Pre-release
- `1.0.0+20251020` - Build metadata

## API Stability Guarantees

### Public API

The **public API** includes all symbols declared in headers under `include/modbuscore/`:

- Function signatures
- Struct layouts (when used by value or pointer dereferencing)
- Enum values
- Macro definitions
- Type aliases

**Guarantee**: No breaking changes within the same MAJOR version.

### Internal API

Files under `src/` are **internal implementation** and may change at any time, even in PATCH releases.

**Guarantee**: None. Do not depend on internals.

### Experimental Features

Features marked `MBC_EXPERIMENTAL_*` or documented as experimental may:
- Change behavior in MINOR releases
- Be removed in MINOR releases (with deprecation warning in prior release)

**Guideline**: Use experimental features only if you can tolerate API churn.

## Breaking Changes

A **breaking change** is any modification that requires users to update their code:

- Removing or renaming public functions
- Changing function signatures (parameters, return type)
- Changing struct layout (for stack-allocated or dereferenced structs)
- Changing enum numeric values (when stored/serialized)
- Removing or renaming macros

### Breaking Change Process

1. **Deprecation** (MINOR release):
   - Mark old API as deprecated using `MBC_DEPRECATED`
   - Add replacement API
   - Update documentation with migration guide
   - Maintain both APIs for at least one MINOR release

2. **Removal** (next MAJOR release):
   - Remove deprecated APIs
   - Update CHANGELOG with migration instructions
   - Provide automated migration tool (if feasible)

### Example Timeline

```
1.0.0 - Initial release with `mbc_old_function()`
1.1.0 - Add `mbc_new_function()`, deprecate `mbc_old_function()`
1.2.0 - Both APIs coexist (users can migrate)
2.0.0 - Remove `mbc_old_function()`
```

## Long-Term Support (LTS)

### LTS Releases

- **Frequency**: Every 2 years
- **Support Duration**: 3 years from release date
- **Versioning**: `X.0.0` (major releases)

### Support Levels

| Version Type | Bug Fixes | Security Patches | New Features | Support Duration |
|--------------|-----------|------------------|--------------|------------------|
| **Current** | ✅ | ✅ | ✅ | Until next MINOR |
| **LTS** | ✅ | ✅ | ❌ | 3 years |
| **Legacy** | ❌ | ✅ (critical only) | ❌ | 1 year after LTS end |
| **EOL** | ❌ | ❌ | ❌ | N/A |

### LTS Roadmap

| Version | Release Date | End of Support | Status |
|---------|-------------|----------------|--------|
| 1.0 LTS | 2025-Q1 | 2028-Q1 | 🚀 Planning |

## Release Process

### Release Cadence

- **MAJOR**: No fixed schedule (when breaking changes accumulate)
- **MINOR**: Every 3-4 months
- **PATCH**: As needed (critical bugs, security issues)

### Release Checklist

1. **Pre-release**:
   - [ ] Update `CHANGELOG.md`
   - [ ] Update version in `CMakeLists.txt`
   - [ ] Run full test suite (all platforms)
   - [ ] Run fuzzing campaign (1B+ executions)
   - [ ] Update documentation
   - [ ] Create migration guide (if breaking changes)

2. **Release**:
   - [ ] Tag release: `git tag -a vX.Y.Z -m "Release X.Y.Z"`
   - [ ] Push tag: `git push origin vX.Y.Z`
   - [ ] GitHub Actions creates release artifacts
   - [ ] Publish to Conan Center
   - [ ] Submit to vcpkg

3. **Post-release**:
   - [ ] Announce on GitHub Discussions
   - [ ] Update documentation site
   - [ ] Monitor issue tracker for regressions

## Compatibility Windows

### Minimum Supported Versions

| Dependency | Minimum Version | Rationale |
|------------|-----------------|-----------|
| CMake | 3.20 | Modern CMake features |
| C Standard | C17 | Standard library features |
| Clang (fuzzing) | 12+ | LibFuzzer support |
| GCC | 9+ | C17 support |
| MSVC | 2019+ | C17 support |

### Platform Support

| Platform | Support Level | Min Version |
|----------|---------------|-------------|
| Linux | Tier 1 | Any modern distro |
| macOS | Tier 1 | 11+ (Big Sur) |
| Windows | Tier 1 | 10+ |
| FreeRTOS | Tier 2 | 10.4+ |
| Zephyr | Tier 2 | 3.2+ |
| Bare Metal | Tier 3 | Embedded ports |

**Tier 1**: Tested in CI, guaranteed to work
**Tier 2**: Community-tested, best-effort support
**Tier 3**: Experimental, may require porting

## Deprecation Policy

### Marking APIs as Deprecated

```c
MBC_DEPRECATED("Use mbc_new_function() instead")
void mbc_old_function(void);
```

### Deprecation Timeline

- **Warning Period**: At least one MINOR release
- **Documentation**: Migration guide in CHANGELOG
- **Removal**: Next MAJOR release

### Example

```
v1.5.0 - Function deprecated, warning added
v1.6.0 - Warning remains
v2.0.0 - Function removed
```

## Security Vulnerabilities

### Reporting

- **Private**: Email `security@modbuscore.example.com`
- **Public**: After patch is released

### Patching Policy

- **Critical**: Patch all supported versions (LTS + Current)
- **High**: Patch LTS + Current
- **Medium/Low**: Patch Current only

### Disclosure Timeline

1. Report received
2. Acknowledge within 48 hours
3. Fix developed and tested
4. Patch released within 30 days (critical) or 90 days (others)
5. Public disclosure 7 days after patch

## Version Upgrade Guide

### Minor Version (e.g., 1.0 → 1.1)

**Expectation**: Drop-in replacement, no code changes required.

**Process**:
1. Update dependency version
2. Rebuild project
3. Test (no API changes expected)

### Major Version (e.g., 1.x → 2.0)

**Expectation**: Breaking changes, code updates required.

**Process**:
1. Read `CHANGELOG.md` and `MIGRATION_GUIDE.md`
2. Fix deprecation warnings in current version first
3. Update dependency version
4. Follow migration guide
5. Update code to use new APIs
6. Rebuild and test thoroughly

## FAQ

**Q: Can I use pre-release versions in production?**  
A: Not recommended. Pre-releases are for testing and feedback only.

**Q: How long will v1.0 be supported?**  
A: 3 years from release date (LTS policy).

**Q: What if I find a bug in an LTS release?**  
A: Report it. We'll backport fixes to LTS branches.

**Q: Can I request a feature in an LTS release?**  
A: No. LTS only receives bug and security fixes. New features go in Current releases.

**Q: How do I know if an API is stable?**  
A: If it's documented in `include/modbuscore/` without `MBC_EXPERIMENTAL_`, it's stable.

---

**Last Updated**: 2025-01-20  
**Next Review**: 2025-07-20
