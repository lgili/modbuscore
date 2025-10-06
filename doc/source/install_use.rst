Install & Use Overview
========================

This page acts as the hub for future getting-started material.  Each section is
currently a scaffold that will be fleshed out as the Gate 17 preparatory work
lands.  Follow the links below to jump to the existing detailed references and
watch for the TODO boxes that highlight pending deliverables.

Quick Start Checklist
---------------------

1. :doc:`installation` – build from source today using the provided CMake
   presets.  Future revisions will attach artefact download links and package
   manager snippets here.
2. :doc:`usage` – review the host-side API walkthrough and transport helper
   examples.  Embedded quickstarts will soon be surfaced from this page once the
   samples under ``embedded/quickstarts`` graduate from placeholders.
3. :doc:`distribution` – track the packaging roadmap.  This section is currently
   a placeholder that enumerates the targets (prebuilt archives, vcpkg, Conan,
   Homebrew, etc.) and will link to installation recipes as they are delivered.

TODOs (Gate 17+)
----------------

* **Prebuilt bundles** – provide download matrices for Linux/macOS/Windows along
  with checksum tables and install scripts.
* **Package feeds** – document the usage of vcpkg, Conan and Homebrew taps once
  the automation jobs are live.
* **Embedded quickstarts** – embed direct links to Zephyr, ESP-IDF and bare-metal
  walkthroughs hosted under ``embedded/``.

Once these items are implemented, this page will evolve into a 60-second tour of
how to fetch the library, wire a transport, and validate the hello-world demos
on both host and target environments.
