Distribution Roadmap
====================

This placeholder tracks how the library will be delivered beyond source builds.
The goal is to make installation frictionless across desktop operating systems,
CI environments, and embedded toolchains.

Prebuilt Archives (Gate 36)
---------------------------

.. todo::

   * Produce platform-specific bundles (Windows ``.zip``, macOS universal
     ``.tar.gz``, Linux ``.tar.gz``) that include shared/static libraries,
     headers, and CMake/pkg-config metadata.
   * Provide verification artefacts (SHA256, signatures) and convenience
     scripts for silent installation.

Package Managers (Gate 37–40)
-----------------------------

.. todo::

   * **vcpkg** – author a ``ports/modbus`` entry with optional features for
     transports and profiles.
   * **Conan** – publish a ``conanfile.py`` with the right CMake generators and
     profile toggles.
   * **Homebrew / Chocolatey / Winget** – document tap/manifest creation and
     validation instructions.

System Integration Guides
-------------------------

.. todo::

   * Document how to reference the installed package from CMake, Meson, Bazel
     and plain Makefiles.
   * Capture CI examples (GitHub Actions, GitLab, Azure Pipelines) that consume
     the published archives or package feeds.

Self-Hosted Mirrors
-------------------

.. todo::

   * Outline the process to mirror the artefacts into an internal package
     registry for air-gapped deployments.
   * Provide templates for generating SBOMs and provenance attestations.

Each section will be expanded as the corresponding gate deliverables ship.
Track progress in :doc:`roadmapping` and the architectural backlog within
``update_plan.md``.
