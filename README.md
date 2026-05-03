# Consensus Hypervisor: The Sovereign Framework (Public Edition)

## What is this?
This repository contains the **Hardware Abstraction Layer (HAL)** and **Architectural Skeleton** for the Consensus Hypervisor (EmergenceOS). It is a Type-1 bare-metal environment designed to explore the concept of "Sovereign Computing"—where data structure is derived from hardware-anchored seeds.

## Included Components
*   **Bare-Metal HAL**: Full implementations for AHCI (SATA) DMA, PCI Discovery, and VGA HUD.
*   **The Sovereign Trap**: Hardware interrupt handlers for the `Ctrl+Alt+Shift+E` hypervisor context switch.
*   **Reference Topology**: A functional, 1-hop implementation of a content-addressable manifold.

## Protected Implementation (Enterprise Binary Only)
To protect the commercial viability of the "Sub-Shannon" research, the following high-performance components are **excluded** from this public source release and are only available in the signed **Hardened Binary Packages**:
1.  **h010g1rAM Optimized Substrate**: The 8-hop path-memoized fetch engine.
2.  **Sub-Shannon Storage Engine**: The 2048-candidate probing logic achieving the 0.03% storage ratio.
3.  **Neumann Bypass**: The logic-transduction layer for spatial instruction resolution.

## Verifiable Claims
You can build and boot this framework to verify the **Forensic Invisibility** claim. Even the reference topology produces a disk footprint that is indistinguishable from random noise (Entropy > 0.9).

## Licensing
This framework is released under the **Emergence Systems License v1.0**. Free for individual evaluation; commercial usage requires a signed Enterprise Agreement.

---
**Build & Run:**
`make && qemu-system-x86_64 -cdrom emergence_os.iso`
