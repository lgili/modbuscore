<!-- OPENSPEC:START -->
# OpenSpec Instructions

These instructions are for AI assistants working in this project.

Always open `@/openspec/AGENTS.md` when the request:
- Mentions planning or proposals (words like proposal, spec, change, plan)
- Introduces new capabilities, breaking changes, architecture shifts, or big performance/security work
- Sounds ambiguous and you need the authoritative spec before coding

Use `@/openspec/AGENTS.md` to learn:
- How to create and apply change proposals
- Spec format and conventions
- Project structure and guidelines

Keep this managed block so 'openspec update' can refresh the instructions.

<!-- OPENSPEC:END -->

# Agents Quick Reference

This repository now has first-class tooling support for AI assistants. Use this
cheat sheet to stay in sync with the existing workflows.

## Build & Package

- Configure/build (single-config generators):
  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
  cmake --build build
  ```
- Install the CMake package (needed by CLI scaffolding & downstream projects):
  ```bash
  cmake --install build --prefix <installdir>
  ```
  This publishes `ModbusCoreConfig.cmake` plus headers.

## Tests

- Run the full suite:
  ```bash
  ctest --test-dir build --output-on-failure
  ```
- The CLI smoke test (`modbus_cli_scaffold`) installs the library into a temp
  prefix and builds a generated project—keep it green when touching templates.

## Code Quality

- Format: `clang-format -i` on all `.c` / `.h` files.
- Static analysis: configure with `-DCMAKE_C_CLANG_TIDY="clang-tidy;-warnings-as-errors=*"`
  or run the CI job (Linux) which already uses that setup.

## Platform Notes

- POSIX examples use `<unistd.h>`; Windows builds skip `modbus_tcp_diagnostics`
  automatically. When adding examples, gate them with `if(NOT WIN32)` if they
  depend on POSIX-only APIs.
- Winsock code should `#define WIN32_LEAN_AND_MEAN`/`NOMINMAX` before including
  `<windows.h>` and keep `<winsock2.h>` included first.

## Scaffold Helpers

- Generate a project with auto-heal enabled:
  ```bash
  python -m tools.modbus_cli.cli new app demo --auto-heal
  ```
- The generated README includes an auto-heal section when enabled; keep the
  generator templates (`tools/modbus_cli/generator.py`) in sync with library APIs.

