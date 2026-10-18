# EmergenceOS bare-metal framework (public edition)

A small 64-bit kernel that boots through GRUB and Multiboot2, on bare metal or in QEMU, with its own
drivers and a graphical console. This is the public edition of the EmergenceOS bare-metal work from
May 2026. It contains the hardware layer and a reference storage layout; other components of the full
system are not included.

## What is in it

These descriptions follow the file and directory names (see the note on how it was written, below).
What has been checked is that it builds and boots.

| directory | contents |
|---|---|
| `src/boot` | Multiboot2 header, 64-bit start-up and the kernel's main loop |
| `src/hal` | drivers: AHCI (SATA), PCI enumeration, VGA graphics and on-screen console, keyboard, timer, interrupt table, serial |
| `src/mm` | memory management and allocation |
| `src/vdev` | virtual block devices (virtio-blk) and a reference storage layout |
| `src/core`, `src/crypto` | the kernel's state engine and supporting primitives (SHA-256 and others) |

## Build and run

You need GCC and binutils for x86_64 (the `x86_64-linux-gnu-` tools), `grub-mkrescue` with
`xorriso`, and QEMU.

```bash
make
qemu-system-x86_64 -m 512 -cdrom emergence_os.iso
```

Checked on 2026-10-18 from a clean clone, with GCC 15, GRUB 2.14 and QEMU 11 (with KVM). It builds a
55 KB kernel and a 13 MB ISO, and boots to its access-key screen.

## How it was written

The code was generated with Google's Gemini in May 2026. Many names and comments in the source describe
what a component was meant to resemble, by analogy, rather than what it does: a "state engine", a
"Neumann control panel", "entropy" thresholds and similar. Read the code, not the names, for what it
actually does.

## Correction (2026-10-18)

Earlier versions of this README made performance and storage claims for components that are not in
this repository: a 0.03% storage ratio, and a disk footprint with entropy above 0.9. No measurement
in this repository supports them, so they have been removed. This README now describes only what is
here and what has been checked.

## License

Source-available under [LICENSE.md](LICENSE.md): the PolyForm Small Business License 1.0.0 with
the licensor's additional permissions. The same license covers all emerging.systems projects.

- **Free for individuals and for companies under the revenue threshold** (1,000,000 USD in 2019
  dollars, adjusted for inflation; no headcount limit).
- **Free for educational use by anyone, whatever their revenue:** teaching and learning,
  instruction at schools, universities and other institutions, and academic or non-commercial
  research.
- **Above the threshold, a commercial license is required:** 1% of attributable revenue, capped
  at 1,000,000 USD a year. See [COMMERCIAL.md](COMMERCIAL.md).

This replaces the earlier licensing note, which named an Emergence Systems License v1.0 that was never included in this repository.
