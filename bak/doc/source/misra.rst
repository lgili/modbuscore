MISRA-C compliance checklist
============================

This library targets a pragmatic subset of MISRA-C:2012 (amendment 2) to
reinforce safety for embedded deployments without compromising portability.
The checklist below summarises the guidance currently enforced by the tooling
chain introduced in Gate 13.

.. list-table:: Summary
   :header-rows: 1
   :widths: 25 50 25

   * - Category
     - Approach
     - Status
   * - Mandatory rules
     - Covered via compiler hardening (``-fstack-protector-strong``,
       ``-fno-common``), ``_FORTIFY_SOURCE=2`` on optimized builds, and the
       clang-tidy security profile.
     - ✅ Enforced by CI
   * - Required rules
     - Checked with the extended clang-tidy configuration
       (``bugprone-*``, ``security-*``, ``cert-*``) plus UBSan/ASan
       configurations in continuous integration.
     - ✅ Enforced by CI
   * - Advisory rules
     - Documented deviations captured below and tracked through code reviews.
     - ⚠️ Monitored manually

Key enforcement hooks
---------------------

* **Static analysis**: the ``.clang-tidy`` profile enables the security and CERT
  bundles, treating ``bugprone-*``, ``clang-analyzer-*`` and ``security-*``
  findings as build failures. The CI "lint" job ensures the checks run on every
  change.
* **Runtime instrumentation**: AddressSanitizer and UndefinedBehaviourSanitizer
  remain enabled in the ``host-asan`` preset. They act as backstops for runtime
  violations associated with MISRA rules.
* **Compiler hardening**: all non-MSVC builds use ``-fstack-protector-strong``
  and ``-fno-common``; Release and RelWithDebInfo configurations additionally
  define ``_FORTIFY_SOURCE=2`` and, on Linux, link with ``-Wl,-z,relro,-z,now``.

Documented deviations
---------------------

The project intentionally relaxes a small set of advisory MISRA rules to keep
portability and ergonomics:

* **Rule 8.4 (external declarations)** – Many public headers expose opaque
  structs for ABI stability, requiring forward declarations that are not paired
  with a definition in the same translation unit. The rule is acknowledged and
  deviation documented in :file:`doc/source/api`.
* **Rule 21.3 (standard library I/O)** – The POSIX transport helper relies on
  ``fcntl``/``ioctl`` for descriptor management; these APIs are wrapped with
  error propagation and are limited to the dedicated port layer.

The remainder of the rule set is either respected by construction or monitored
by the combined static analysis, sanitizers, and unit tests. Future deviations
will be appended to this section with a rationale and regression tests.

Maintenance checklist
---------------------

To keep compliance up to date:

* Run ``cmake --preset host-asan`` followed by ``ctest`` to validate sanitizer
  builds before tagging releases.
* Review clang-tidy reports locally via
  ``cmake --preset host-debug -DCMAKE_C_CLANG_TIDY=clang-tidy`` prior to
  submitting patches that touch core FSM or port layers.
* Update this checklist whenever new tooling or rule deviations are introduced.
