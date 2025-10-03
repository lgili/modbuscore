Contributing
============

Bug reports, feature requests, and pull requests are welcome. Please:

* Open an issue describing the motivation and scope.
* Follow the coding style enforced by the repository (C11, Clang/GCC warnings).
* Add unit/integration tests whenever the behaviour changes.
* Run `cmake --preset host-debug && cmake --build --preset host-debug && ctest --output-on-failure --test-dir build/host-debug` before submitting.

The full contribution guidelines live in `CONTRIBUTING.md` in the repository root.
