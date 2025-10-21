# Support

Thank you for using ModbusCore! This document explains how to get help, report issues, and contribute to the project.

---

## 📞 Getting Help

### Before You Ask

1. **Check the documentation**: [docs/](docs/) and [README.md](README.md)
2. **Search existing issues**: [GitHub Issues](https://github.com/lgili/modbuscore/issues)
3. **Review examples**: [examples/](examples/)
4. **Read troubleshooting guide**: [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)

### Support Channels

| Channel | Purpose | Response Time |
|---------|---------|---------------|
| [GitHub Issues](https://github.com/lgili/modbuscore/issues) | Bug reports, feature requests | 1-3 business days |
| [GitHub Discussions](https://github.com/lgili/modbuscore/discussions) | Questions, best practices, show & tell | Best effort |
| [Documentation](https://modbuscore.readthedocs.io) | API reference, guides, examples | Always available |

### What to Include in Your Question

**For bug reports**, include:
- ModbusCore version (`git describe --tags`)
- Platform (OS, compiler, architecture)
- Minimal reproducible example
- Expected vs actual behavior
- Relevant logs/error messages

**For feature requests**, include:
- Clear use case description
- Why existing features don't work
- Proposed API (if you have ideas)
- Impact on your project

**For questions**, include:
- What you're trying to achieve
- What you've already tried
- Code snippets (minimal)

---

## 🐛 Reporting Issues

### Bug Reports

Use the **Bug Report** template when creating an issue:

```markdown
**Describe the bug**
A clear description of what the bug is.

**To Reproduce**
Steps to reproduce:
1. Configure runtime with...
2. Call function...
3. Observe error...

**Expected behavior**
What you expected to happen.

**Environment**
- ModbusCore version: [e.g., 1.0.0]
- OS: [e.g., Ubuntu 22.04]
- Compiler: [e.g., GCC 11.3]
- Architecture: [e.g., x86_64]

**Additional context**
Logs, stack traces, etc.
```

### Feature Requests

Use the **Feature Request** template:

```markdown
**Problem**
Describe the problem this feature would solve.

**Solution**
Describe your proposed solution.

**Alternatives**
Alternatives you've considered.

**Additional context**
Any other context, mockups, examples.
```

---

## 🔒 Security Vulnerabilities

**⚠️ Do NOT report security vulnerabilities in public issues!**

### Reporting Process

1. **Email**: `security@modbuscore.example.com` (or direct to maintainers)
2. **Include**:
   - Description of vulnerability
   - Steps to reproduce
   - Potential impact
   - Affected versions
3. **Wait for acknowledgment** (within 48 hours)
4. **Coordinate disclosure** (we prefer 90-day embargo)

### What Qualifies as a Security Issue?

- Memory corruption (buffer overflow, use-after-free, etc.)
- Remote code execution
- Denial of service
- Authentication/authorization bypass
- Information disclosure

### What Doesn't

- Bugs that only affect development builds
- Issues requiring physical access
- Theoretical vulnerabilities without proof of concept

See [SECURITY.md](SECURITY.md) for our security policy.

---

## 🤝 Contributing

We welcome contributions! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Quick Start

1. **Fork** the repository
2. **Create** a feature branch: `git checkout -b feature/my-feature`
3. **Make** your changes
4. **Test**: `cmake --build build && ctest --test-dir build`
5. **Format**: `clang-format -i` on modified files
6. **Commit**: Follow [Conventional Commits](https://www.conventionalcommits.org/)
7. **Push**: `git push origin feature/my-feature`
8. **Open** a Pull Request

### Contribution Types

- 🐛 **Bug fixes**: Always welcome
- ✨ **Features**: Discuss in an issue first
- 📝 **Documentation**: Typos, clarifications, examples
- 🧪 **Tests**: Improve coverage
- 🚀 **Performance**: Benchmarks and optimizations
- 🔧 **Tooling**: CI/CD, build system improvements

---

## 📅 Support Schedule

### Version Support

| Version | Status | Bug Fixes | Security | End of Support |
|---------|--------|-----------|----------|----------------|
| 1.0 LTS | Current | ✅ | ✅ | 2028-Q1 |
| 0.x | Legacy | ❌ | ❌ | 2025-Q1 (EOL) |

### Response Times

| Issue Type | First Response | Resolution Target |
|------------|----------------|-------------------|
| Security (critical) | 24 hours | 7 days |
| Security (high) | 48 hours | 30 days |
| Bug (critical) | 24 hours | 14 days |
| Bug (normal) | 3 business days | Best effort |
| Feature request | 1 week | Backlog |
| Question | Best effort | N/A |

**Note**: These are targets, not guarantees. We're a volunteer-driven project.

---

## 🌍 Community Guidelines

### Code of Conduct

We follow the [Contributor Covenant Code of Conduct](https://www.contributor-covenant.org/version/2/1/code_of_conduct/).

**In short:**
- Be respectful and inclusive
- Welcome newcomers
- Accept constructive criticism
- Focus on what's best for the community

### Communication Standards

- **Be patient**: Maintainers are volunteers
- **Be specific**: Vague questions get vague answers
- **Be grateful**: Say thanks when someone helps
- **Give back**: Help others when you can

---

## 📚 Resources

### Documentation

- **API Reference**: [docs/api/](docs/api/)
- **Architecture**: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- **Examples**: [examples/](examples/)
- **Troubleshooting**: [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)
- **Fuzzing**: [FUZZING.md](FUZZING.md)
- **Versioning**: [VERSIONING.md](VERSIONING.md)

### External Resources

- **Modbus Specification**: [modbus.org](https://modbus.org/specs.php)
- **CMake Documentation**: [cmake.org/documentation](https://cmake.org/documentation/)
- **C17 Standard**: [ISO/IEC 9899:2018](https://www.iso.org/standard/74528.html)

---

## 🗺️ Public Roadmap

Our roadmap is tracked in [roadmap.md](roadmap.md) and [GitHub Projects](https://github.com/lgili/modbuscore/projects).

### Current Phase (2025-Q1)

- ✅ Phase 1-6: Core architecture, transport, runtime
- ⏳ Phase 7: Robustness, diagnostics, security
- 🚀 Phase 8: Distribution, packaging, release

### Upcoming (2025-Q2)

- Additional function codes (FC01, FC02, FC04, FC05)
- Enhanced FreeRTOS support
- Performance benchmarks

### Future

- TLS/DTLS support
- Language bindings (Python, Rust)
- Modbus+ protocol

**Want to influence the roadmap?** Vote on [feature request issues](https://github.com/lgili/modbuscore/labels/feature-request)!

---

## 📧 Contact

- **General questions**: [GitHub Discussions](https://github.com/lgili/modbuscore/discussions)
- **Bug reports**: [GitHub Issues](https://github.com/lgili/modbuscore/issues)
- **Security**: `security@modbuscore.example.com`
- **Maintainers**: [@lgili](https://github.com/lgili)

---

## 🙏 Acknowledgments

ModbusCore is maintained by volunteers. If you find it useful, consider:

- ⭐ **Star** the repository
- 🐛 **Report** bugs
- 📝 **Improve** documentation
- 💬 **Help** others in discussions
- 💰 **Sponsor** the project (if you can)

---

**Thank you for being part of the ModbusCore community! 🚀**

---

*Last updated: 2025-01-20*
