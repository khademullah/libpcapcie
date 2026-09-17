# Security Policy

## Reporting a security issue

Please do not open a public issue for a suspected security vulnerability.
Instead, contact the maintainers privately through the repository security reporting workflow if available.

## Supported versions

This project is an early-stage library and support is currently best-effort. Please treat it as a prototype and validate hardware access carefully in trusted environments.

## Hardware safety guidance

- PCIe access can affect live hardware and system configuration.
- Run validation workloads in a controlled environment.
- Be careful with privileged access to sysfs and device configuration.
- Avoid exposing hardware interfaces to untrusted environments.
