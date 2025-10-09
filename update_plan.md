Let's architect ModbusCore (The Core of Modern Modbus Communication) as a production-grade product: modular, portable, deadlock-free, fully tested, and well documented. Below is an incremental plan split into objective gates—move forward only when the gate is green.

Vision & principles
	•	Portable: “pure” C11 (C99 fallback), no malloc by default (optional pool), zero hard dependencies.
	•	Modular: clearly split PDU codec, Transport (RTU/TCP/ASCII), Core FSM (client/server), HAL/Port (UART/TCP, timers), Buffers/Queues, and Logging.
	•	Deadlock-proof: fully non-blocking, per-state timeouts, internal watchdog, transaction queue with retries and cancellation.
	17) Embedded DX bundle (integration in minutes)
		• Objective: make embedding the library into existing firmware trivial.
		• Deliverables:
		• embedded/quickstarts/ with three paths:
		1.	Drop-in single file: modbus_amalgamated.c/.h (client, baseline RTU, PDU core).
		2.	Component packages: components/esp-idf/modbus/ (CMakeLists, menuconfig), zephyr/module.yml + Kconfig.modbus.
		3.	Vendor HAL samples: ports/stm32-ll-dma-idle/, ports/nxp-lpuart-idle/ with UART RX DMA + IDLE-line integration.
		• Progress 2025-10-06: Added `scripts/amalgamate.py` generator and published the `quickstarts/drop_in` pair so firmware teams can copy two files for a baseline RTU client.
		• Progress 2025-10-06: Published ESP-IDF (`quickstarts/components/esp-idf/modbus`) and Zephyr (`quickstarts/components/zephyr`) quickstart packages bundling transport adapters around the amalgamated client.
		• Progress 2025-10-07: Drafted vendor HAL samples in `quickstarts/ports/stm32-ll-dma-idle` and `quickstarts/ports/nxp-lpuart-idle` showing DMA+IDLE hooks, ISR glue, and request helpers wired to the drop-in client.
		• Progress 2025-10-07: Polished STM32/NXP helpers, added the Renesas RL78 SCI quickstart, refreshed the quickstart index, and wired everything to the new `mb_embed` request shims. Hardware validation of the ESP-IDF hello-RTU and Zephyr hello-TCP tracks remains.
		• embedded/porting-wizard.md: one-page checklist (clock/baud/timers/now_ms/yield/ISR vs thread).
		• include/modbus/mb_embed.h: ergonomic shims (builder/parser helpers + queue wrappers).
		• Gate: run hello-RTU on ESP-IDF (hardware UART) and hello-TCP on Zephyr (sockets) by following only the quickstart README, with no changes to the core library.

	⸻

	18) TINY/LEAN/FULL profiles + fine-grained FC feature flags
		• Objective: keep footprint under control and compile only what’s needed.
		• Deliverables:
		• MB_PROFILE={TINY,LEAN,FULL} plus granular MB_ENABLE_FCxx toggles (03/04/05/06/0F/10/17, etc.).
		• MODBUS_ENABLE_{CLIENT,SERVER,RTU,TCP,ASCII} with const tables in .rodata.
		• CI script that reports ROM/RAM per profile (host and target, e.g., STM32G0/ESP32C3).
		• Gate: automated report shows TINY ≤ x KiB ROM / ≤ y B RAM with limited FCs; FULL retains performance with no regressions.
		• Progress 2025-10-08: Tightened TINY defaults to register-only FCs, introduced feature guards across headers/server paths, published the runtime feature descriptor (`modbus/features.h`), and added `scripts/ci/report_footprint.py` to emit ROM/RAM summaries per profile.
		• Progress 2025-10-09: CI `footprint` job runs the host MinSizeRel sweep, verifies the README snapshot, and uploads a JSON artifact with current numbers.

	⸻

	19) Budgeted FSM micro-steps
		• Objective: cooperate cleanly with main loops/RTOS schedulers.
		• Deliverables:
		• mb_client_poll_with_budget(c, steps) and mb_server_poll_with_budget(s, steps).
		• Sub-states: RxHeader→RxBody→Validate→Dispatch and TxBuild→TxSend→TxDrain.
		• Per-substate deadlines + jitter monitor (sampling now_ms() between steps).
		• Gate: under stress (30% drop/dup), poll() jitter stays under the target and no stalls occur; the system watchdog never fires.

	⸻

	20) Real-world RTU UART: DMA + IDLE line + T3.5
		• Objective: robust frame-end detection with minimal ISR overhead.
		• Deliverables:
		• Reference port ports/stm32-ll-dma-idle/ (circular DMA, IDLE ISR marks boundaries, timer enforces T1.5/T3.5).
		• Zero-copy consumption of the DMA buffer via indices (no memcpy).
		• “RTU Timing Pitfalls” guide with T1.5/T3.5 tables per baud/parity.
		• Gate: HIL at 19 200/38 400/115 200 8E1 with bursts: zero overruns, correct framing in >99.9% even with injected noise.
		• Progress 2025-10-07: Guard-time enforcement added to the STM32 helper (auto T1.5/T3.5 derivation + overrides), documentation refreshed, and a HIL validation playbook published to drive the acceptance matrix.

	⸻

	21) Zero-copy IO & scatter-gather
		• Objective: reduce RAM usage and copies during TX/RX.
		• Deliverables:
		• mb_iovec_t {const uint8_t* base; size_t len;} in mb_transport_if.h.
		• Encoders write directly into the TX ring; parser reads windows from the RX ring (views).
		• memcpy counters in tests (macro hook).
		• Gate: integration demonstrates >90% fewer memcpy calls in hot paths; scratch memory per transaction stays < 64 B.

	⸻

	22) Lightweight SPSC/MPSC + static pool
		• Objective: fixed latency and zero malloc.
		• Deliverables:
		• Lock-free SPSC (ISR producer, thread consumer) and MPSC with short critical sections.
		• Transaction/ADU pool with freelist and high-water-mark tracking.
		• Gate: stress 1 M transactions without leaks (Valgrind on host), TSan/ASan clean, latency variation stays under the target.

	⸻

	23) Optional ISR-safe mode
		• Objective: support fast half-duplex links and short turnarounds.
		• Deliverables:
		• mb_on_rx_chunk_from_isr(ptr,len) + mb_try_tx_from_isr().
		• MB_IN_ISR() guards to suppress heavy logging and avoid locks.
		• Gate: TX-after-RX turnaround < 100 µs @ 72 MHz (reference board).

	⸻

	24) Backpressure + simple QoS
		• Objective: avoid head-of-line blocking and overflowing queues.
		• Deliverables:
		• Queue with two classes (high/normal) and policy by FC or deadline.
		• Immediate rejection for “queue full (non-critical)” paths.
		• Gate: chaos tests show critical FCs never exceed latency targets under load.

	⸻

	25) Compact on-device diagnostics
		• Objective: enable debugging without a heavy console.
		• Deliverables:
		• uint16_t counters per event (timeouts, CRC, exceptions, retries).
		• Circular trace buffer (64 events) with low-cost event codes.
		• mb_diag_snapshot() for application shell debugging.
		• Gate: overhead <0.5% CPU (LEAN profile), plus a guide on interpreting the counters.

	⸻

	26) Interop rig & golden pcaps
		• Objective: prove compatibility with the broader ecosystem.
		• Deliverables:
		• Docker environment with modpoll, pymodbus, libmodbus, freemodbus; capture .pcapng per scenario/FC/error.
		• CI test that compares responses and MBAP/TID values.
		• Gate: interop matrix is 100% green; regressions block merges.

	⸻

	27) Power & tickless real-time
		• Objective: fit battery-powered devices / tickless RTOS setups.
		• Deliverables:
		• Idle callback (queue empty, no RX): application can down-clock/sleep.
		• Examples using compare-match timers; zero busy-waiting.
		• Gate: measurements show expected power drop; no frame loss when waking on IDLE/RX events.

	⸻

	28) Reproducible benchmarks (host + target)
		• Objective: evolve without regressions.
		• Deliverables:
		• Microbenchmarks (cycles/frame) on host (clock_gettime) and target (DWT cycle counter).
		• Scenarios: encode, parse, end-to-end RTT, CPU load vs baud.
		• Charts published in CI.
		• Gate: PRs fail if they degrade key metrics beyond the allowed budget.

	⸻

	29) Canonical “Embedded Ready” examples
		• Objective: provide copy-paste starting points for firmware developers.
		• Deliverables:
		• Bare-metal main loop: RTU client with poll_with_budget(4) and a 1 ms tick.
		• FreeRTOS: modbus_rx/modbus_tx/app tasks, IDLE ISR notifications, use of stream buffers.
		• Zephyr: k_work + k_timer with Kconfig integration.
		• ESP-IDF: UART driver + IDLE event + ready-to-use component.
		• Gate: each example has a one-page Getting Started guide and passes basic HIL tests.

	⸻

	30) Footprint & worst-case stack guide
		• Objective: provide predictable resource usage.
		• Deliverables:
		• Table of ROM/RAM per profile and worst-case stack per path (RX/TX/error).
		• Recommendations for configTOTAL_HEAP_SIZE/stack per task.
		• Gate: verified on two MCUs (Cortex-M0+ / M4F) with documentation listing reproducible numbers.

	⸻

	31) Kconfig / menuconfig & build presets
		• Objective: friendly configuration without editing headers.
		• Deliverables:
		• Kconfig.modbus (Zephyr) and menuconfig (ESP-IDF) exposing feature flags, profiles, queue depth, timeouts.
		• toolchains.cmake + preset-* for common targets (STM32G0, ESP32C3, nRF52).
		• Gate: toggles are reflected in binaries (map file) and in CI footprint reports.

	⸻

	32) Amalgamated build + package install
		• Objective: integrate in two clicks.
		• Deliverables:
		• scripts/amalgamate.py generates modbus_amalgamated.[ch] (selected profile).
		• pkg-config + find_package(modbus) support (host and cross builds).
		• Gate: “drop-in UART” demo builds with only the two files; pkg-config example links on Linux/Windows.

	⸻

	33) Security and robustness for noisy links
		• Objective: resilient RTU operation.
		• Deliverables:
		• Fast resync (scan by address field + CRC precheck).
		• Recent-duplicate filter (lightweight ADU hash per window).
		• Fault-injection tests (bit flips, truncation, phantom bytes).
		• Gate: success >99% even with 30% loss and random bit flips.

	⸻

	34) Opt-in “libmodbus-like” compatibility layer
		• Objective: migrate existing projects without major rewrites.
		• Deliverables:
		• Thin wrappers with similar signatures (where it makes sense) mapping to the modern API.
		• “Port from libmodbus” guide with an equivalence table.
		• Gate: migration demo runs without app changes (aside from includes) for FC 03/06/16.

	⸻

	Checklists destined for embedded/ (copy-paste into projects)

	Port RTU UART (DMA + IDLE)
		•	Configure circular RX DMA (256–512 B buffer).
		•	Enable the IDLE interrupt and clear USARTx->ICR = USART_ICR_IDLECF.
		•	Timer compare for T1.5/T3.5 (baud-aware).
		•	Call mb_on_rx_chunk_from_isr(ptr,len) from IDLE/TC handlers.
		•	Call mb_client_poll_with_budget() from the main loop or k_work.

	Choose profile and flags
		•	MB_PROFILE=TINY (production) / LEAN (default) / FULL (debug/PC).
		•	Enable only the FCs you use (MB_ENABLE_FC03/06/16).
		•	MODBUS_ENABLE_RTU (yes) / TCP (as needed).

	Guarantee predictable latency
		•	Define poll_budget (e.g., 4–8 steps per tick).
		•	Configure per-FC timeouts and queue priority for critical work.
		•	Monitor jitter (diagnostics enabled in development, disabled in production).

	⸻

	Quick implementation notes to avoid perf/footprint regressions
		•	Use restrict in safe encoders/parsers; static inline on hot paths; noinline on error paths.
		•	MB_LIKELY/UNLIKELY on critical checks.
		•	Tables in const (flash); avoid blanket memset—initialise only what’s needed.
		•	CRC with nibble-table (16 entries) in TINY; 256-entry table in FULL.

	⸻

	Perfect! Let’s add a cross-platform “distribution rail” so anyone can install and use the library on Windows, macOS, and Linux without pain. I kept the same Objective / Deliverables / Gate style and continued numbering after 34.

	⸻

	35) CMake Package (config-package) + pkg-config
		• Objective: standard consumption via find_package() and pkg-config.
		• Deliverables:
		• cmake/modbusConfig.cmake, modbusConfigVersion.cmake, modbusTargets.cmake exported with install(EXPORT ...).
		• modbus.pc (pkg-config) with Cflags, Libs, static/shared variants.
		• GNUInstallDirs + correct layout: headers in include/modbus, libs in lib{,64}, CMake files in lib/cmake/modbus.
		• MODBUS_BUILD_SHARED/MODBUS_BUILD_STATIC, MODBUS_INSTALL=ON.
		• Consumption examples: examples/cmake-consume/ and examples/pkgconfig-consume/.
		• Gate: cmake -S examples/cmake-consume -Dmodbus_DIR=<prefix>/lib/cmake/modbus builds on MSVC/Windows, clang/macOS, gcc/Linux; pkg-config --cflags --libs modbus works on Linux/macOS.

	⸻

	36) Prebuilt artifacts per platform (triple-OS CI)
		• Objective: ship ready-to-use binaries with a single command.
		• Deliverables:
		• GitHub Actions matrix produces artifacts:
		• Windows: .zip with modbus.dll + modbus.lib + headers + CMake package.
		• macOS (universal): .tar.gz with universal libmodbus.dylib (arm64+x86_64) + headers + CMake package.
		• Linux: .tar.gz with libmodbus.so + headers + CMake package + modbus.pc.
		• scripts/install-from-archive.{ps1,sh} to drop into a prefix and refresh CMake/pkg-config caches.
		• Gate: download & install the artifact, then build examples/cmake-consume on each OS without rebuilding the library.

	⸻

	37) vcpkg port (Windows/macOS/Linux)
		• Objective: “vcpkg install modbus” just works.
		• Deliverables:
		• ports/modbus/portfile.cmake and CONTROL/vcpkg.json with features (rtu, tcp, tiny, full).
		• CI job testing: vcpkg install modbus[rtu] --triplet x64-windows, x64-osx, x64-linux.
		• Doc: docs/install-vcpkg.md.
		• Gate: consumer example builds with -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake on all three platforms.

	⸻

	38) Conan package (1.x/2.x)
		• Objective: integrate with the Conan ecosystem.
		• Deliverables:
		• conanfile.py with options (shared, rtu, tcp, profile) and cmake_find_package/CMakeDeps.
		• Pipeline that publishes to the Conan local cache (and instructions for remotes).
		• Example examples/conan-consume/ (CMake).
		• Gate: conan create . modbus/1.0.0@ + conan install ... + build of the example succeeds on Windows/macOS/Linux.

	⸻

	39) Homebrew (macOS) + dedicated tap
		• Objective: brew install <tap>/modbus.
		• Deliverables:
		• Formula/modbus.rb (consumes release .tar.gz), with depends_on "cmake" => :build.
		• Tap github.com/<org>/homebrew-modbus.
		• Usage doc and --HEAD option for development builds.
		• Gate: brew tap <org>/modbus && brew install <org>/modbus/modbus installs successfully; pkg-config modbus works.

	⸻

	40) Chocolatey/Winget (Windows) or lightweight equivalent
		• Objective: easy Windows installation outside of vcpkg.
		• Deliverables (graduated options):
		• Phase A (fast path): install.ps1 downloads the release artifact, expands to %ProgramFiles%\modbus, and registers modbus_DIR in the registry for CMake.
		• Phase B (optional): Chocolatey package (tools/chocolateyInstall.ps1) pointing to the release artifact.
		• Optional Winget manifest.
		• Gate: .\scripts\install.ps1 -Version 1.0.0 followed by cmake -S examples/cmake-consume -Dmodbus_DIR="%ProgramFiles%\modbus\cmake" builds successfully with MSVC.

	⸻

	41) Meson wrap / pkg-config only (Linux friendly)
		• Objective: support Meson projects with minimal effort.
		• Deliverables:
		• Simple meson.build with default_library=both.
		• Local wrapdb (subprojects/modbus.wrap) and instructions for dependency('modbus') via pkg-config.
		• Gate: meson setup build && meson compile -C build runs the example using pkg-config.

	⸻

	42) Installer UX & “hello” examples per OS
		• Objective: remove friction for people who just want to try it.
		• Deliverables:
		• Windows: hello-rtu-win.sln (MSBuild) linking against modbus.lib from the artifact in C:\modbus\.
		• macOS: hello-tcp-macos.sh (clang + pkg-config).
		• Linux: hello-rtu-linux.sh (gcc + pkg-config) plus a udev hint for /dev/ttyUSBx.
		• Gate: each script compiles/runs the “hello” example on CI VMs (GitHub Actions runners).

	⸻

	43) Signing / notarization (minimum viable)
		• Objective: reduce security warnings on macOS/Windows.
		• Deliverables:
		• macOS: pipeline target for codesign --sign <identity> (when certificates are available), with ad-hoc instructions otherwise.
		• Windows: optional signtool step (when certificates are available).
		• Gate: binaries signed whenever possible, plus fallback documentation (hashes + ReleaseNotes).

	⸻

	44) Documentation: “install and use in 60 seconds”
		• Objective: painless onboarding.
		• Deliverables:
		• docs/install.md page with four tracks: Prebuilt, vcpkg, Conan, Build from source.
		• Ready-to-use snippets:
		• CMake:

	find_package(modbus 1.0 CONFIG REQUIRED)
	target_link_libraries(app PRIVATE modbus::modbus)


		• pkg-config:

	cc hello.c $(pkg-config --cflags --libs modbus) -o hello


		• Compatibility table (MSVC 2019/2022, Clang, GCC) and required flags.

		• Gate: user test (external dev) reports successful installation on each OS using only the documentation.

	⸻

	Quick consumption tips (to include in the README)
		• Windows (MSVC + prebuilt)

	set MODBUS_DIR=C:\modbus\1.0.0
	cmake -S examples/cmake-consume -B build -Dmodbus_DIR=%MODBUS_DIR%\lib\cmake\modbus
	cmake --build build --config Release


		• macOS (Homebrew)

	brew tap <org>/modbus
	brew install <org>/modbus/modbus
	cmake -S examples/cmake-consume -B build
	cmake --build build


		• Linux (pkg-config)

	sudo tar -C /usr/local -xzf modbus-1.0.0-linux-x86_64.tar.gz
	cc hello.c $(pkg-config --cflags --libs modbus) -o hello




