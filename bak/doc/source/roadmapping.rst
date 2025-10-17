Roadmap & Gates (0–44)
======================

This section tracks the full roadmap from Gate 0 through Gate 44. Completed
milestones (Gate 0–15) document the functionality available today, while the
subsequent sections outline planned work that is already scoped in
``update_plan.md``. Each gate lists the objectives, key deliverables and the
acceptance checks that feed the test suite or CI.

Gate 0 – Foundation & Build
---------------------------

* **Objective:** Initialise the project structure and toolchain presets.
* **Deliverables:** CMake presets for GCC/Clang/MSVC, warning flags
  (``-Wall -Wextra -Werror``), baseline CI workflow, Doxygen/Sphinx skeleton.
* **Gate:** CI must build on Linux, macOS and Windows (GitHub Actions matrix)
  and the documentation target must succeed.

Gate 1 – Core Types & Utilities
-------------------------------

* **Deliverables:** ``mb_types.h`` (fixed width aliases), ``mb_err.h``, logging
  macros, ring buffer (SPSC), CRC16 helper.
* **Gate:** 100 % unit-test coverage for utils (`ctest` target) and clean runs
  under ASan/UBSan (``cmake --preset host-asan``).

Gate 2 – PDU Codec
------------------

* **Deliverables:** Encode/decode for FC 03/06/16 plus exception helpers and
  payload limit checks (≤ 253 bytes).
* **Gate:** Table-driven unit tests (good/bad vectors) with ≥ 90 % coverage and
  5 min fuzz run without crashes (``tests/fuzz`` harness).

Gate 3 – Transport Abstraction
------------------------------

* **Deliverables:** ``mb_transport_if_t`` (non-blocking send/recv), frame helper
  ``mb_frame_rtu_*``.
* **Gate:** Transport mock + frame helpers exercised in integration tests
  (`test_transport_if`, `test_modbus_core`).

Gate 4 – RTU Minimum
--------------------

* **Deliverables:** Stateful RTU parser with inter-byte silence detection and
  transmit helper.
* **Gate:** Loopback tests (mock/UART simulator) covering echo, byte loss and
  duplication (`test_rtu_transport`).

Gate 5 – Client FSM
-------------------

* **Deliverables:** Non-blocking transaction manager (Idle→Sending→Waiting→
  Done/Timeout/Retry/Abort), exponential backoff with jitter, cancellation
  support.
* **Gate:** Stress test with 1 000 sequential requests and simulated packet
  loss (``MbClientTest`` suite); watchdog remains idle in nominal cases.

Gate 6 – Server Core
--------------------

* **Deliverables:** Register-region mapping with callbacks/storage, FC 03/06/16
  handling, exception validation.
* **Gate:** Client↔server integration (loopback) covering read/write/multi-write
  and malformed frames (`test_client_server_integration`, `test_modbus_server`).

Gate 7 – TCP / MBAP
-------------------

* **Deliverables:** MBAP framing, TID ↔ transaction mapping, optional
  multi-connection helper (`mb_tcp_multi_*`), TCP CLI example.
* **Gate:** CI job spins up a Python Modbus TCP server and validates behaviour
  using the library CLI plus ``mbtget``/``modpoll``; throughput mock processes
  ≥ 1 000 responses (`test_tcp_throughput`).

Gate 8 – Robustness & Backpressure
----------------------------------

* **Deliverables:** Queue backpressure (configurable capacity), poison-pill
  drains, per-function timeout overrides, latency/response metrics for both
  client and server.
* **Gate:** Chaos tests introduce bursty traffic, latency and packet loss;
  the metrics counters capture drops/timeouts and no starvation occurs.

Gate 9 – Port/HAL Adapters
--------------------------

* **Deliverables:** POSIX socket wrapper, FreeRTOS stream/queue adapter and
  bare-metal transport helper (non-blocking UART/SPI integrations), plus a
  portable mutex shim.
* **Gate:** POSIX and FreeRTOS examples compile/run, bare-metal adapter passes
  hardware-in-the-loop smoke tests.

Gate 10 – Extended Function Codes + ASCII
-----------------------------------------

* **Deliverables:** Helpers for FC 01/02/04/05/0F/17 alongside validation and
  regression tests, plus Modbus ASCII framing with LRC validation.
* **Gate:** Unit/integration tests cover the additional PDU helpers and ASCII
  transport, ensuring malformed requests raise the correct exceptions.

Gate 11 – Observability & Debug
-------------------------------

* **Deliverables:** Structured events (state enter/leave, transaction lifecycle),
  opt-in hex tracing, diagnostics counters grouped by function code and error
  bucket (``mb_diag_*`` API).
* **Gate:** Integration tests capture events while inducing failures, and diag
  counters reflect errors/timeouts accurately.

Gate 12 – Documentation & API Stability
---------------------------------------

* **Deliverables:** Ports guide, migration notes, client/server cookbook,
  Sphinx/Doxygen refresh and SemVer 1.0.0 declaration (stable umbrella header
  ``<modbus/modbus.h>``).
* **Gate:** Documentation builds without warnings, examples compile, and the
  API surface is frozen under SemVer guarantees.

Gate 13 – Hardening & Release
-----------------------------

* **Deliverables:** clang-tidy security profile, MISRA-C checklist, compiler
  hardening flags (``-fno-common``, ``-fstack-protector-strong``,
  ``_FORTIFY_SOURCE=2``) and ELF RELRO/Now when available.
* **Gate:** CI runs sanitizers, fuzzers, coverage and linting; release tags are
  cut from green pipelines.

Gate 14 – Protocol Parity & Data Helpers
----------------------------------------

* **Objective:** Close the feature gap with incumbent Modbus stacks and simplify popular data handling flows.
* **Deliverables:**
  - Byte-order conversion helpers (``modbus_get_uint32_*``, ``modbus_get_float_*`` and matching setters) covering ABCD/DCBA/BADC/CDAB permutations.
  - Golden tests that validate the new helpers against libmodbus fixtures and ensure symmetry between getters and setters.
  - Documentation updates (README parity table and cookbook snippets) guiding migrations that rely on mixed-endian payloads.
* **Gate:** Byte-order helpers round-trip against libmodbus vectors and the transport demos confirm identical payload layouts across ASCII, RTU and TCP.

Gate 15 – Compatibility & Port Conveniences
-------------------------------------------

* **Objective:** Provide drop-in ergonomics for teams migrating from libmodbus and other legacy stacks.
* **Deliverables:**
  - ``mb_server_mapping_init`` and ``mb_server_mapping_apply`` helpers bundling transport configuration, register banks and request pools.
  - Packaging artefacts for both ``find_package(modbuscore CONFIG REQUIRED)`` and ``pkg-config --cflags --libs modbuscore`` consumers.
  - ``modbus_unit_test_loop_demo`` example mirroring the libmodbus unit-test flow on top of the cooperative FSMs.
* **Gate:** Packaging examples build on Linux, macOS and Windows while the mapping helper passes the parity suite used by libmodbus.

Gate 16 – Reserved Bridge Milestone
-----------------------------------

This gate is intentionally reserved for a future release that will bridge the
hardening work from Gate 13 with the embedded developer experience delivered in
Gate 17. Details will be published once the scope is finalised.

Gate 17 – Embedded DX Bundle (Integration in Minutes)
-----------------------------------------------------

* **Objective:** Make embedding the library into existing firmware trivial.
* **Deliverables:**
  - ``embedded/quickstarts/`` drop-in single translation unit pair (``modbus_amalgamated.c/.h``) for a baseline RTU client.
  - Quickstart component packages for ESP-IDF and Zephyr wrapping transports, configuration defaults and build glue.
  - Vendor HAL samples (STM32 LL DMA IDLE, NXP LPUART IDLE, Renesas RL78 SCI) illustrating DMA+IDLE integration.
  - ``embedded/porting-wizard.md`` checklist that guides UART, timer and watchdog bring-up end to end.
  - Ergonomic shims in ``include/modbus/mb_embed.h`` for builder/parser helpers plus queue wrappers.
* **Gate:** Following the quickstart README, engineers can run hello-RTU on ESP-IDF hardware UART and hello-TCP on Zephyr without touching core sources.

Gate 18 – Profile Matrix & Feature Flags
----------------------------------------

* **Objective:** Keep footprint under control and compile only what each product requires.
* **Deliverables:**
  - ``MB_PROFILE={TINY, LEAN, FULL, CUSTOM}`` selector with granular ``MB_ENABLE_FCxx`` toggles.
  - ``MODBUS_ENABLE_{CLIENT,SERVER,RTU,TCP,ASCII}`` guards consolidating feature tables in ``.rodata``.
  - CI footprint reporter (``scripts/ci/report_footprint.py``) emitting ROM/RAM measurements per profile and target.
* **Gate:** Automated reports prove the TINY profile stays within the agreed ROM/RAM budgets while FULL retains performance with no regressions.

Gate 19 – Budgeted FSM Micro-Steps
----------------------------------

* **Objective:** Cooperate cleanly with cooperative schedulers and RTOS task budgets.
* **Deliverables:**
  - ``mb_client_poll_with_budget()`` and ``mb_server_poll_with_budget()`` entry points that cap per-tick work.
  - Rx/Tx substates (RxHeader→RxBody→Validate→Dispatch and TxBuild→TxSend→TxDrain) with explicit hand-offs.
  - Per-substate deadlines plus a jitter monitor sampling ``now_ms()`` across iterations.
* **Gate:** Under stress (30 % drop/dup), poll jitter stays under target and watchdog instrumentation never fires.

Gate 20 – Real-World RTU UART: DMA + IDLE Line + T3.5
----------------------------------------------------

* **Objective:** Deliver a robust frame-end detector with minimal ISR overhead.
* **Deliverables:**
  - Reference port at ``ports/stm32-ll-dma-idle/`` using circular DMA, IDLE ISR markers and timers enforcing T1.5/T3.5.
  - Zero-copy consumption of DMA buffers through index management.
  - “RTU Timing Pitfalls” guide listing T1.5/T3.5 tables by baud/parity.
* **Gate:** Hardware-in-the-loop runs at 19 200/38 400/115 200 baud (8E1) complete with zero overruns and >99.9 % correct framing even under injected noise.

Gate 21 – Zero-Copy IO & Scatter-Gather
---------------------------------------

* **Objective:** Reduce RAM usage and eliminate redundant copies during TX/RX.
* **Deliverables:**
  - ``mb_iovec_t`` fragments in ``mb_transport_if.h`` representing scatter/gather windows.
  - Encoders writing directly into the TX ring buffer; parsers consuming views from the RX ring.
  - Optional memcpy counters toggled via test hooks to quantify improvements.
* **Gate:** Integration tests demonstrate ≥ 90 % fewer memcpy calls on hot paths and scratch buffers stay below 64 B per transaction.

Gate 22 – Lightweight SPSC/MPSC + Static Pool
---------------------------------------------

* **Objective:** Guarantee fixed latency and zero dynamic allocation.
* **Deliverables:**
  - Lock-free SPSC queue for ISR producer / thread consumer plus an MPSC variant with short critical sections.
  - Transaction/ADU pool with freelist management and high-watermark tracking.
  - Instrumentation to assert no pool leaks under extended stress.
* **Gate:** One-million-transaction stress run completes without leaks; TSan/ASan stay clean and latency variation remains within budget.

Gate 23 – Optional ISR-Safe Mode
--------------------------------

* **Objective:** Support fast half-duplex links and short turnaround times.
* **Deliverables:**
  - ``mb_on_rx_chunk_from_isr()`` and ``mb_try_tx_from_isr()`` hooks.
  - ``MB_IN_ISR()`` guards that suppress heavy logging and avoid locks.
  - Documentation guiding when to enable ISR mode and how to back off to task context.
* **Gate:** Reference boards achieve < 100 µs TX-after-RX turnaround at 72 MHz with ISR mode enabled.

Gate 24 – Backpressure + Simple QoS
-----------------------------------

* **Objective:** Avoid head-of-line blocking and keep queues bounded.
* **Deliverables:**
  - Dual-class queue (high/normal) with policy by function code or deadline.
  - Immediate rejection path for non-critical work when queues are full.
  - Chaos test suite introducing bursty traffic, latency spikes and backpressure assertions.
* **Gate:** Critical function codes never exceed latency targets under load and queues remain stable in chaos tests.

Gate 25 – Compact On-Device Diagnostics
---------------------------------------

* **Objective:** Enable field diagnostics without a heavyweight console.
* **Deliverables:**
  - Lightweight counters (timeouts, CRC, exceptions, retries) stored as ``uint16_t``.
  - Circular trace buffer (64 events) with low-cost event codes.
  - ``mb_diag_snapshot()`` API for shell-friendly snapshots.
* **Gate:** LEAN profile overhead stays below 0.5 % CPU and documentation explains how to interpret the counters.

Gate 26 – Interop Rig & Golden PCAPs
------------------------------------

* **Objective:** Prove compatibility with the broader Modbus ecosystem.
* **Deliverables:**
  - Docker environment bundling modpoll, pymodbus, libmodbus and freemodbus.
  - Scripts capturing ``.pcapng`` per scenario/function-code/error pair.
  - CI job comparing responses and MBAP/TID values, blocking regressions.
* **Gate:** Interop matrix is 100 % green; any deviation fails the pipeline.

Gate 27 – Power & Tickless Real-Time
------------------------------------

* **Objective:** Fit battery-powered or tickless RTOS deployments.
* **Deliverables:**
  - Idle callback for queue-empty/no-RX states so applications can downclock or sleep.
  - Examples using compare-match timers to replace busy waiting.
  - Guides for integrating with low-power frameworks on popular MCUs.
* **Gate:** Power measurements confirm the expected drop and no frames are lost when waking on IDLE/RX events.

Gate 28 – Reproducible Benchmarks (Host + Target)
-------------------------------------------------

* **Objective:** Guard against performance regressions.
* **Deliverables:**
  - Microbenchmarks capturing cycles/frame on host (``clock_gettime``) and target (DWT counter).
  - Scenarios covering encode, parse, end-to-end RTT and CPU load vs baud.
  - CI charts plotting metrics over time.
* **Gate:** Pull requests fail if they degrade key benchmark metrics beyond agreed budgets.

Gate 29 – Canonical “Embedded Ready” Examples
---------------------------------------------

* **Objective:** Offer copy-paste starting points for firmware developers.
* **Deliverables:**
  - Bare-metal main loop showing RTU client with ``poll_with_budget(4)`` and a 1 ms tick.
  - FreeRTOS example wiring ``modbus_rx``, ``modbus_tx`` and application tasks using stream buffers.
  - Zephyr and ESP-IDF examples aligned with vendor build systems and quickstart packages.
* **Gate:** Each example ships with a one-page Getting Started guide and passes hardware-in-the-loop smoke tests.

Gate 30 – Footprint & Worst-Case Stack Guide
-------------------------------------------

* **Objective:** Provide predictable resource usage.
* **Deliverables:**
  - Tabulated ROM/RAM per profile plus worst-case stack per path (RX/TX/error).
  - Recommendations for ``configTOTAL_HEAP_SIZE`` and per-task stacks.
  - Scripts extracting size data from map files for reproducibility.
* **Gate:** Numbers are verified on two MCUs (Cortex-M0+ and Cortex-M4F) and published alongside reproduction steps.

Gate 31 – Kconfig/Menuconfig & Build Presets
--------------------------------------------

* **Objective:** Simplify configuration without editing headers directly.
* **Deliverables:**
  - ``Kconfig.modbus`` (Zephyr) and ESP-IDF ``menuconfig`` definitions exposing feature flags, profiles, queue depth and timeouts.
  - ``toolchains.cmake`` plus preset files for common targets (STM32G0, ESP32C3, nRF52).
  - Wiring that propagates configuration into CI footprint reports.
* **Gate:** Toggling options via UI is reflected in binaries and reported by the footprint job.

Gate 32 – Amalgamated Build + Package Install
---------------------------------------------

* **Objective:** Enable “integrate in two files” workflows.
* **Deliverables:**
  - ``scripts/amalgamate.py`` producing ``modbus_amalgamated.c/.h`` for selected profiles.
  - Updated install rules exporting both pkg-config and CMake targets for host and cross builds.
  - Drop-in UART demo proving the amalgamated pair builds without the full tree.
* **Gate:** pkg-config and CMake consumption examples link successfully on Linux and Windows using only the generated artifacts.

Gate 33 – Security & Robustness for Noisy Links
-----------------------------------------------

* **Objective:** Increase resilience on degraded RTU links.
* **Deliverables:**
  - Fast resync logic scanning address fields with CRC prechecks.
  - Recent-duplicate filter using lightweight ADU hashes.
  - Fault-injection tests covering bit flips, truncation and phantom bytes.
* **Gate:** Test matrix achieves >99 % success even with 30 % packet loss and random bit flips.

Gate 34 – Opt-In libmodbus Compatibility Layer
----------------------------------------------

* **Objective:** Ease migrations from the classic libmodbus API.
* **Deliverables:**
  - Thin wrapper layer mapping familiar signatures onto the modern API.
  - “Port from libmodbus” guide with equivalence tables.
  - Integration tests exercising FC 03/06/16 via the compatibility shim.
* **Gate:** Migration demo runs without application changes (aside from includes) for the covered function codes.

Gate 35 – CMake Package & pkg-config
------------------------------------

* **Objective:** Offer standard consumption paths for build systems.
* **Deliverables:**
  - Exported CMake package (``modbusTargets.cmake``, ``modbusConfig.cmake``, ``modbusConfigVersion.cmake``) installed under GNUInstallDirs.
  - ``modbuscore.pc`` definition containing Cflags/Libs for shared and static variants.
  - Examples under ``examples/cmake-consume/`` and ``examples/pkgconfig-consume/``.
* **Gate:** ``cmake -S examples/cmake-consume`` succeeds across GCC/Clang/MSVC using the installed package; ``pkg-config --cflags --libs modbuscore`` works on Linux and macOS.

Gate 36 – Prebuilt Artifacts per Platform
-----------------------------------------

* **Objective:** Ship ready-to-use binaries through CI.
* **Deliverables:**
  - GitHub Actions matrix producing Windows ``.zip``, macOS universal ``.tar.gz`` and Linux ``.tar.gz`` archives with headers and CMake/pkg-config metadata.
  - ``scripts/install-from-archive.ps1`` and ``.sh`` helpers that expand into a prefix and refresh caches.
  - Release notes documenting artifact contents and checksums.
* **Gate:** Downloading an artifact, installing it and building ``examples/cmake-consume`` works on each supported OS without rebuilding the library.

Gate 37 – vcpkg Port
--------------------

* **Objective:** Enable ``vcpkg install modbus`` on Windows, macOS and Linux.
* **Deliverables:**
  - ``ports/modbus/portfile.cmake`` and ``vcpkg.json`` with feature flags (rtu, tcp, tiny, full).
  - CI job validating ``vcpkg install modbus[rtu]`` for x64-windows, x64-osx and x64-linux triplets.
  - ``docs/install-vcpkg.md`` with usage instructions.
* **Gate:** vcpkg consumers build the examples using the vcpkg toolchain file across all three operating systems.

Gate 38 – Conan Package
-----------------------

* **Objective:** Integrate with Conan 1.x and 2.x workflows.
* **Deliverables:**
  - ``conanfile.py`` exposing options for shared/static builds, protocols and profiles.
  - Pipeline that publishes to the local cache and documents remote publication steps.
  - ``examples/conan-consume/`` demonstrating CMake integration via Conan generators.
* **Gate:** ``conan create . modbus/1.0.0@`` followed by ``conan install`` and ``cmake --build`` succeeds on Windows, macOS and Linux.

Gate 39 – Homebrew Tap
----------------------

* **Objective:** Provide ``brew install <tap>/modbus`` for macOS users.
* **Deliverables:**
  - Homebrew formula consuming the release tarball and depending on CMake for builds.
  - Tap repository (``github.com/<org>/homebrew-modbus``) with audit scripts.
  - Documentation covering stable and ``--HEAD`` installs.
* **Gate:** ``brew tap <org>/modbus && brew install <org>/modbus/modbus`` installs successfully and exposes pkg-config metadata.

Gate 40 – Windows Installers (Chocolatey/Winget)
-----------------------------------------------

* **Objective:** Offer painless Windows installation beyond vcpkg.
* **Deliverables:**
  - ``scripts/install.ps1`` fast path that downloads release artifacts, installs under ``%ProgramFiles%\\modbus`` and registers the CMake package location.
  - Optional Chocolatey package (``tools/chocolateyInstall.ps1``) referencing the released artifact.
  - Winget manifest when certificates and distribution policy allow.
* **Gate:** Running the installer followed by ``cmake -S examples/cmake-consume -Dmodbuscore_DIR="%ProgramFiles%\\modbus\\cmake"`` builds with MSVC without rebuilding the library.

Gate 41 – Meson Wrap & pkg-config
---------------------------------

* **Objective:** Support Meson projects with minimal friction.
* **Deliverables:**
  - ``meson.build`` with ``default_library=both`` and exported dependency metadata.
  - ``subprojects/modbus.wrap`` for wrapdb style consumption alongside pkg-config integration.
  - Example project proving the dependency flow.
* **Gate:** ``meson setup build && meson compile -C build`` runs successfully using the pkg-config integration.

Gate 42 – Installer UX & “Hello” Examples per OS
------------------------------------------------

* **Objective:** Remove friction for users evaluating the library.
* **Deliverables:**
  - Windows solution (``hello-rtu-win.sln``) linking against the installed artifact in ``C:\\modbus\\``.
  - macOS script (``hello-tcp-macos.sh``) leveraging clang + pkg-config on universal binaries.
  - Linux script (``hello-rtu-linux.sh``) plus udev hints for ``/dev/ttyUSBx``.
* **Gate:** Each script builds and runs on GitHub Actions runners for the respective OS.

Gate 43 – Signing & Notarisation
--------------------------------

* **Objective:** Reduce security warnings on macOS and Windows.
* **Deliverables:**
  - macOS codesign/notarisation pipeline hooks (ad-hoc when certificates are unavailable).
  - Windows ``signtool`` integration for signed DLLs/lib files when certificates are provisioned.
  - Documentation providing checksum-based verification fallback.
* **Gate:** Distributed binaries are signed whenever possible; fallback guidance is published alongside releases.

Gate 44 – Documentation: Install & Use in 60 Seconds
----------------------------------------------------

* **Objective:** Deliver painless onboarding across distribution channels.
* **Deliverables:**
  - ``docs/install.md`` describing prebuilt, vcpkg, Conan and build-from-source flows.
  - Ready-to-use snippets for ``find_package(modbuscore)`` plus pkg-config compilation.
  - Compatibility matrix (MSVC 2019/2022, Clang, GCC) with required flags.
* **Gate:** External user testing confirms successful installation on each OS using only the documentation.
