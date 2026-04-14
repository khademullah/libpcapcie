# libpcapcie

libpcapcie is a minimal, Scapy-like PCIe packet crafting, exercising, and
analysis framework written in C.

Features (skeleton):
- Layered TLP objects
- Filter with callback sniffing API
- Pluggable backends (dummy, FPGA, VFIO, etc.)

This repo provides structure, not full PCIe Gen5 compliance.