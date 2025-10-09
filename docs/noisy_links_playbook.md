# Noisy Link Acceptance Playbook

This plays back the Gate 33 scenarios so QA can reproduce the fast-resync and duplicate-filter behaviour under adverse line conditions.

## Test Matrix

| Scenario | Injected Fault | Expected Behaviour | Verification Steps |
| --- | --- | --- | --- |
| Retry after timeout | Drop 1 response out of 3 | Client resends, duplicate filter remembers request hash but does not block original response | `tests/test_dup_filter.cpp::RealWorldScenario_Retransmission` |
| Line reflections | Echoed frame 5 ms after original | Duplicate filter catches second frame, stats increment | `tests/test_dup_filter.cpp::RealWorldScenario_LineReflection` |
| Partial frame corruption | CRC flip mid-stream | Resync discards bytes until next address; CRC precheck rejects frame | `tests/test_rtu_resync.cpp::ResyncScenario_CorruptedFrameRecovery` |
| Random noise burst | 30 % bit flips across burst | Resync eventually finds next valid frame, recovered counter increments | Integration/HIL placeholder |

## Tooling

* Fault injection helpers live under `tests/fault_inject.*` and drive targeted corruption in unit tests.
* For hardware loops, reuse the `examples/rtu_loop_demo.c` binary with UART shims that call into `mb_dup_filter_add()` and resync APIs.

## Metrics to Monitor

* `mb_dup_filter_get_stats()` — ensure `duplicates_found` tracks injected artefacts and `false_positives` stays zero during the matrix.
* `mb_rtu_resync_get_stats()` — confirm `frames_recovered` increments whenever the recovery path assembles a full frame after noise.
* Application-level retries must stay below the 1 % target during the matrix; otherwise revisit T3.5 guard configuration.

## Next Steps

* Capture serial traces (Saleae / logic analyzer) for each scenario and store in `Testing/noisy-links/` once real hardware runs.
* Promote the playbook into the CI `noise` job once the HIL rig is wired in (see Gate 40).
