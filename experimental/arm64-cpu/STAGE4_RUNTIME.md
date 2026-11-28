# Stage 4 runtime prototype

The experimental AG64 image format is a deliberately restricted test format.
It is not an ELF, Mach-O, SELF or PS5 executable loader.

## AG64 binary layout

All integer fields are little-endian.

| Offset | Width | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII AG64 |
| 4 | 4 | Version 1 or 2 |
| 8 | 4 | Code length in bytes |
| 12 | 4 | Initialized data length |
| 16 | 4 | Entry offset into code |
| 20 (v2 only) | 4 | Relocation record count |
| 20 (v1) / 24 (v2) | variable | Code, then data |
| After data (v2) | count x 8 | Relocation records |

A version 2 relocation consists of a 32-bit code patch offset followed by
a 32-bit data target offset. The loader replaces four code bytes with the
absolute guest data address (0x1000 plus target offset). Out-of-range
patches and targets are rejected. No symbol linking, external dependencies,
PC-relative relocation or ELF parsing is implemented.

The interpreter uses a five-byte near-jump trampoline before the loaded
code to select the entry offset. Therefore code addresses within the
interpreter have a five-byte bias compared with AG64 code offsets.
Cross-function relative branches entirely inside the code retain their
displacements; absolute code addresses are not relocated.

## Memory layout

Code is stored separately from guest data and cannot be modified by guest
data-memory instructions. Initialized data starts at 0x1000, followed by a
4096-byte unmapped guard region, then a bounded guest stack. Region
enforcement is opt-in for the underlying GuestCpu and enabled by GuestRuntime.
The format currently requires data sizes divisible by four.

## Validation and limits

CTest guest_runtime covers entry selection, data initialization, mapped
stack access, unmapped memory rejection, malformed headers and version 2
relocations. Stage 3's 2,047 compiled cases remain a separate regression gate.
This prototype is not yet a complete sandbox or production untrusted-code
executor; it has no syscalls, general virtual memory manager, process ABI,
dynamic linker or OS emulation.
