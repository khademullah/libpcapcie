# libpcapcie v1.0.0

## Overview

This is the first stable release of the libpcapcie project, covering the complete base application and tooling for PCIe trace capture, decode, and analysis.

The release focuses on the core project as a usable, buildable, and maintainable PCIe exploration and trace platform. This includes the GUI, shared library, backend abstraction, examples, and support scripts required to run the project as a complete software stack.

## Included in this release

- PCIe trace capture and playback support
- Trace file open/save workflow
- Packet inspection and row-level detail view
- TLP summary and live trace monitoring
- Backend abstraction for PCIe enumeration and trace sources
- GUI viewer for the complete project workflow
- Example applications and utilities for device/trace testing
- Cross-platform build support via CMake and project scripts

## Scope and intent

This first release is intentionally centered on the complete project foundation rather than experimental or specialized AI-only functionality.

The AI PCIe emulator, topology-specific experimental flows, and extended benchmarking scenarios are not treated as the core release feature set. They remain optional, separate, and should be considered experimental additions rather than the primary product definition of this release.

## Notable stability improvements

- GUI readability and selection visibility fixes
- Clear separation between standard trace view and optional additional logs
- Better handling for separate PCIe enumeration capture workflows
- Cleaner startup and shutdown behavior for the Zephyr/QEMU-backed topology tooling
- Improved script validation for environment checks and stale process cleanup

## Build and usage

The project is designed to build as a standard CMake-based workspace. Refer to the repository documentation and example tools for local setup and usage.

## Release status

- Status: first stable project release
- Version: v1.0.0
- Focus: complete project baseline, not AI-emu-specific feature marketing
