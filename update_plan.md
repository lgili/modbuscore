Let's architect this C Modbus library as a production-grade product: modular, portable, deadlock-free, fully tested, and well documented. Below is an incremental plan split into objective gates—move forward only when the gate is green.

Vision & principles
	•	Portable: “pure” C11 (C99 fallback), no malloc by default (optional pool), zero hard dependencies.
	•	Modular: clearly split PDU codec, Transport (RTU/TCP/ASCII), Core FSM (client/server), HAL/Port (UART/TCP, timers), Buffers/Queues, and Logging.
	•	Deadlock-proof: fully non-blocking, per-state timeouts, internal watchdog, transaction queue with retries and cancellation.
	•	Safe: strict bounds and opcode validation, PDU size limits (≤ 253 bytes), CRC/MBAP guards, overflow/underflow checks, UB-safe code paths.
	•	Observable: metrics, structured events, log levels, tracing hooks.
	•	Testable: TDD, fuzzers on decoders, sanitizers, coverage, and CI coverage.

⸻

Layered architecture

app (examples) ─────────────────────────────────────────────────────┐
                                                                    │
             ┌───────────── Core FSM ───────────────┐               │
             │  Client (master)  |  Server (slave)                 │
             │  Transactions, timeouts, retries, watchdog          │
             └──────────────────────────────────────┘               │
             ┌────────────  PDU Codec  ────────────┐                │
             │  encode/decode FCs, exceptions, limits               │
             └──────────────────────────────────────┘               │
             ┌──────────  Transport abstraction ───┐                │
             │  RTU | ASCII | TCP (plug-ins)        │               │
             └──────────────────────────────────────┘               │
             ┌───────────  Port/HAL  ───────────────┐               │
             │  UART/TCP, timers, monotonic clock, optional mutex   │
             └──────────────────────────────────────┘               │
             ┌────────── Utilities: Buffers/Queue/CRC/Log ─┐        │
             └─────────────────────────────────────────────┘        │

	•	Core FSM: poll-based finite state machine for Rx/Tx, per-transaction deadlines.
	•	PDU Codec: builders/parsers (FC03/04/06/16/…), exception helpers, limit tables.
	•	Transport: RTU (CRC/silence timing), TCP (MBAP/TID), ASCII (optional).
	•	Port/HAL: function pointers for read, write, now_ms, sleep(0), yield(), tick source, optional locks.
	•	Utilities: lock-free SPSC ring buffer, fixed pool, CRC16, MBAP helpers, logger façade.

⸻

Repository layout

/include/modbus/          # public headers
/src/core/                # FSM, transaction manager
/src/pdu/                 # encoders/decoders
/src/transport/rtu/       # RTU transport
/src/transport/tcp/       # TCP transport
/src/port/                # reference ports (posix, freertos, bare)
/src/util/                # crc, ring buffer, pool, log
/examples/                # client/server demos
/tests/unit/              # unit tests
/tests/fuzz/              # libFuzzer/AFL harnesses
/docs/                    # doxygen + sphinx (breathe)
/cmake/                   # toolchains, options

⸻

Incremental roadmap with gates

Each stage lists Objective, Deliverables, and Gate (acceptance). We only advance when the gate is satisfied.

0) Foundation & build
	•	Objective: project skeleton, build options, coding style.
	•	Deliverables: CMake + presets (gcc/clang/msvc), -Wall -Wextra -Werror, MODBUS_* options, clang-format, basic Doxygen, GitHub Actions CI matrix.
	•	Gate: CI green across three toolchains, style checks pass, docs generated.

1) Types, errors, logging, utilities
	•	Deliverables: mb_types.h (fixed types), mb_err.h, MB_LOG(level, ...), SPSC ring buffer, CRC16 helpers.
	•	Gate: 100% unit coverage on utilities; ASan/UBSan clean runs.

2) PDU codec (core)
	•	Deliverables: encode/decode FC 0x03, 0x06, 0x10 + exceptions; enforce PDU ≤ 253.
	•	Gate: golden-table tests (valid/invalid vectors), coverage ≥ 90%, fuzz decoders for 5 minutes without crashes.

3) Transport abstraction
	•	Deliverables: mb_transport_if.h (non-blocking send/recv), mb_frame.h scaffolding.
	•	Gate: transport mock passes integration tests with the PDU layer.

4) Minimal RTU
	•	Deliverables: RTU framing (address, PDU, CRC), stateful parser with 3.5 char silence timeout, inter-char TX pacing.
	•	Gate: loopback against mock port and simulated UART; lost/duplicated byte tests pass.

5) Client core FSM
	•	Deliverables: transaction queue, Idle→Sending→Waiting→Done/Timeout/Retry/Abort states, exponential backoff retries, cancel support.
	•	Gate: stress tests (1k requests), simulated packet loss, zero deadlocks; watchdog never fires in happy path.

6) Server core
	•	Deliverables: register table with read/write callbacks, range checks, exception generation.
	•	Gate: basic compliance (local client ↔ server) with FC 03/06/16; invalid frame injection maps to correct exceptions.

7) TCP (MBAP)
	•	Deliverables: MBAP header (TID, length, unit), TID↔transaction mapping, optional multi-connection helper.
	•	Gate: CI integration tests against mbtget/modpoll (docker); stable throughput at 1k rps (mock).

8) Advanced robustness
	•	Deliverables: backpressure (queue caps), anti-head-of-line priorities, per-FC timeouts, poison pill flush, metrics (counters/latency).
	•	Gate: chaos tests (latency/bursts/loss 20–40%), no starvation, valgrind clean.

9) Port/HALs
	•	Deliverables: port_posix, port_freertos, port_bare (tick + UART/TCP), optional locks (C11 atomics fallback).
	•	Status: ✅ POSIX sockets, bare-metal tick adapter, FreeRTOS stream/queue shim, and portable mutexes delivered.
	•	Gate: examples run on POSIX and simulated FreeRTOS.

10) ASCII (optional) + extra FCs
	•	Deliverables: ASCII framing; extend FC coverage (01/02/04/05/0F/17 as needed).
	•	Status: 🔄 FC helpers landed (validated requests/responses); ASCII framing still pending.
	•	Gate: same regression suite as RTU/TCP.

11) Observability & debug
	•	Deliverables: state enter/leave events, optional hex trace, observability callback, error/function counters, MB_DIAG plumbing.
	•	Gate: example surfaces useful metrics and diagnoses induced failures.

12) Documentation & API stability
	•	Deliverables: porting guide, migration guide, cookbook (client/server), complete Doxygen, SemVer 1.0.0 release.
	•	Gate: docs build warning-free; examples build and run; breaking-change review yields zero blockers.

13) Hardening & release
	•	Deliverables: security-focused clang-tidy profile, MISRA-C checklist (non-strict), -fno-common, -fstack-protector, FORTIFY_SOURCE when available.
	•	Gate: full CI (sanitizers + fuzz + coverage + linters) green; tag v1.0.0.
	•	Status: ✅ clang-tidy tightened, hardening flags enabled, coverage/fuzz/lint jobs active in CI.

14) Protocol parity & data helpers
	•	Deliverables: close remaining FC gaps (0x05/0x07/0x11/0x16/0x17), extend exception catalog, add endian conversion helpers (float/int setters/getters mirroring libmodbus).
	•	Gate: test matrix covering all new FCs, documentation updated with support table, API comparison versus libmodbus.
	•	Status: ✅ deliverables shipped and documented.

15) Compatibility & port convenience
	•	Deliverables: quick mapping constructors (libmodbus `modbus_mapping_new` style), auto-generated `pkg-config`/`find_package` artifacts, migration guide, examples equivalent to classic libmodbus client/server tests.
	•	Gate: examples build using only the installed package, migration guide covers 90% of libmodbus→modbus workflows, integration pipelines validate both demos.

16) Footprint & feature toggles
	•	Deliverables: build flags to enable/disable client/server/transports, footprint documentation, CI presets validating combinations (client-only, server-only), flash/RAM usage metrics.
	•	Gate: dedicated builds pass regression suite with single component enabled, report demonstrates ≥20% footprint reduction on reference targets.

⸻

Deadlock-proof & packet loss strategies
	•	Non-blocking polling: mb_client_poll()/mb_server_poll() advance the FSM in tiny steps.
	•	Per-state deadlines (timestamp recorded on entry).
	•	Internal watchdog: if a state exceeds N×timeout → abort transaction, increment metric, return to Idle.
	•	Exponential retries with jitter; per-transaction and per-window limits.
	•	Transaction queue with fixed-size backpressure, cancellation, priority support.
	•	SPSC ring buffer and buffer pool (malloc-free) to avoid fragmentation.
	•	Validation: ADU/PDU bounds, CRC/MBAP, address/count ranges, alignment checks.
	•	Fail-fast: immediate error reporting and diagnostic events—never swallow failures silently.

⸻

Quality, tests & CI
	•	Unit: cmocka/ceedling-style coverage for utilities, codec, FSM.
	•	Integration: in-process client↔server with transport mocks plus RTU/TCP real flows (docker modbus-tk, mbserver).
	•	Fuzzing: libFuzzer/AFL targeting PDU/RTU/TCP decoders.
	•	Sanitizers: ASan/UBSan/TSan (where feasible).
	•	Coverage: gcov/lcov (target ≥ 85%).
	•	Static: clang-tidy, cppcheck (security rules), MISRA subset.
	•	CI: gcc/clang/msvc matrix; release/debug/sanitize builds; doc & example artifacts.

⸻

API sketch (draft)

/* include/modbus/modbus.h */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_PDU_MAX 253

typedef enum {
    MB_OK = 0,
    MB_EINVAL,
    MB_EBUSY,
    MB_EAGAIN,
    MB_ETIME,
    MB_ECRC,
    MB_EIO,
    MB_EPROTO,
    MB_ENOSPC,
    MB_ESTATE,
} mb_err_t;

/* Forward declarations */
struct mb_port;
struct mb_client;
struct mb_server;

/* Timestamps in milliseconds */
typedef uint64_t mb_time_ms_t;

/* Transport-neutral send/recv (non-blocking) */
typedef struct {
    mb_err_t (*send)(void *ctx, const uint8_t *buf, size_t len);
    mb_err_t (*recv)(void *ctx, uint8_t *buf, size_t cap, size_t *out_len);
} mb_transport_if_t;

/* Port/HAL */
typedef struct mb_port {
    void *user;                          /* UART/TCP handle, etc. */
    mb_transport_if_t io;                /* send/recv */
    mb_time_ms_t (*now_ms)(void);        /* monotonic clock */
    void (*yield)(void);                 /* optional CPU hint */
} mb_port_t;

/* Client transaction handle */
typedef struct {
    uint16_t tid;        /* for TCP; 0 for RTU */
    uint8_t  unit_id;
    uint8_t  fc;         /* function code */
    const uint8_t *pdu;  /* request PDU payload (after FC) */
    size_t   pdu_len;
    uint32_t timeout_ms;
    uint8_t *resp_buf;   /* provided by caller or pool */
    size_t   resp_cap;
    size_t   resp_len;   /* filled on completion */
    mb_err_t status;
} mb_txn_t;

/* Lifecycle */
mb_err_t mb_client_create(mb_client **out, const mb_port_t *port, size_t queue_depth);
mb_err_t mb_client_destroy(mb_client *c);

/* Non-blocking submit */
mb_err_t mb_client_submit(mb_client *c, mb_txn_t *txn);
/* Poll advances the FSM; call from main loop or timer */
mb_err_t mb_client_poll(mb_client *c);

/* Helpers: common builders (FC 03/06/16) */
mb_err_t mb_build_read_holding(uint16_t addr, uint16_t qty, uint8_t *out_pdu, size_t *inout_len);
mb_err_t mb_parse_read_holding_resp(const uint8_t *pdu, size_t len, const uint8_t **out_bytes, size_t *out_len);

/* Server */
typedef mb_err_t (*mb_srv_read_cb)(uint16_t addr, uint16_t qty, uint8_t *out_bytes, size_t out_cap);
typedef mb_err_t (*mb_srv_write_cb)(uint16_t addr, const uint8_t *bytes, size_t len);

mb_err_t mb_server_create(mb_server **out, const mb_port_t *port);
mb_err_t mb_server_set_callbacks(mb_server *s, mb_srv_read_cb rd, mb_srv_write_cb wr);
mb_err_t mb_server_poll(mb_server *s);

#ifdef __cplusplus
}
#endif

Non-blocking RTU client example:

/* application loop */
mb_client *cli;
mb_port_t port = my_rtu_port();
mb_client_create(&cli, &port, /*queue*/ 8);

uint8_t pdu[5];
size_t pdu_len = sizeof pdu;
mb_build_read_holding(0x0000, 2, pdu, &pdu_len);

uint8_t resp[64];
mb_txn_t t = {
    .unit_id = 1,
    .fc = 0x03,
    .pdu = pdu,
    .pdu_len = pdu_len,
    .timeout_ms = 200,
    .resp_buf = resp,
    .resp_cap = sizeof resp
};
mb_client_submit(cli, &t);

for (;;) {
    mb_client_poll(cli);
    if (t.status == MB_OK && t.resp_len) {
        const uint8_t *bytes;
        size_t n;
        mb_parse_read_holding_resp(t.resp_buf + 1, t.resp_len - 1, &bytes, &n);
        /* process bytes... */
    }
    app_do_other_work();
}

⸻

Documentation
	•	Doxygen for the API plus Sphinx (Breathe) guides (Port/HAL, cookbook, troubleshooting).
	•	Examples: RTU/TCP client and server; POSIX and FreeRTOS variants.
	•	CHANGELOG (SemVer), CONTRIBUTING, SECURITY.md, Design.md (FSM + diagrams).

⸻

Suggested two-week sprint slices
	1.	Days 1–3: Stages 0–1 (foundation, utilities, logging) + CI.
	2.	Days 4–7: Stages 2–3 (PDU codec + transport mock) with TDD and basic fuzzing.
	3.	Days 8–10: Stages 4–5 (RTU + client FSM) + chaos testing.
	4.	Days 11–14: Stage 6 (server) + initial docs and examples.

⸻

High-value extras
	•	Expose cmake/Kconfig options to toggle FCs and transports.
	•	Amalgamated distribution (single .c + .h) for quick integration.
	•	Audit hooks (pre-send/post-receive) suitable for tracing/pcap capture.
	•	Stub generator script for custom FCs.

⸻