Developer Guide
===============

Repository Layout
-----------------

``modbus/``
    Library sources and public headers.
``examples/``
    Reference applications for POSIX, FreeRTOS and Zephyr.
``tests/``
    GoogleTest suites covering state machines, transports and ports.
``interop/``
    Integration fixtures used in CI against third-party stacks.
``benchmarks/``
    Performance micro-benchmarks.
``docs/`` and ``doc/``
    Sphinx documentation and high-level design notes.

Coding Standards
----------------

* C11 with optional C99 fallback (no dynamic allocation inside the FSMs).
* ``clang-format`` style is enforced; run ``scripts/format-all.sh`` before
  submitting patches.
* Warning-free builds at ``-Wall -Wextra -Werror`` across GCC/Clang/MSVC are
  required.
* New features must be guarded by ``MB_CONF_*`` macros to keep portability.

Testing
-------

Run the full suite before publishing a branch:

.. code-block:: bash

   cmake --build --preset host-debug
   ctest --preset all --output-on-failure
   cmake --build --preset host-asan
   ctest --preset all --output-on-failure

Use ``host-msvc`` on Windows.  The ``interop`` tests can be executed locally via
``scripts/run-interop.sh`` which pulls the Docker images used in CI.

Continuous Integration
-----------------------

GitHub Actions runs the following stages:

* ``linux-clang`` / ``linux-gcc`` – host debug, ASan/UBSan, benchmarks.
* ``windows-msvc`` – MSVC build, tests, packaging.
* ``interop`` – runs the Modbus TCP/RTU fixtures inside Docker.
* ``docs`` – builds HTML docs and publishes to GitHub Pages.

Roadmap & Issues
----------------

The ``GATExx_PLAN.md`` files describe ongoing work.  When proposing a feature:

1. Reference the next gate or create a new gate document.
2. Open an issue with the expected configuration flags and tests.
3. Provide documentation updates alongside code changes.

Contribution Checklist
----------------------

* Code follows style and passes ``clang-format``.
* Unit tests cover the new behaviour; integration tests updated when necessary.
* Sphinx documentation updated (:doc:`overview`, :doc:`getting_started`, etc.).
* README and changelog entries added if the public API changes.

License
-------

The project is released under the MIT License (:doc:`license`).  Contributions
are accepted under the same terms.  Include a Signed-off-by line if your
organisation requires it (DCO style).
