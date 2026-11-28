# libkernel

## change [sceKernelGetProsperoSystemSwVersion]

- [sceKernelGetProsperoSystemSwVersion](../../../core/libs/prx/libkernel/System/src/SoftwareVersion.cpp) (libkernel) formats and packs one configured guest firmware version, defaulting to `01.000.000` (`0x01000000`). PS5 payload measurements record `11.600.005`, `13.420.000` and `13.600.007`. Firmware 13.60 preserves the 64-bit size field for 0, 0x27, 0x28, 0x29, 0x30, 0x100 and 2^64-1 and writes bytes 8 through 0x27. A null pointer returns 0 without changing errno. An unwritable pointer returns `SCE_KERNEL_ERROR_EFAULT` as a host-safety fallback. Both firmware queries return `SCE_KERNEL_ERROR_EINVAL` when they need a malformed host setting. Title processes, unwritable console pointers and other firmware versions were not measured. `sceKernelGetSystemSwVersion` exposes a distinct version on measured firmware and still reports its own baseline.

# libkernel_sys

## change [sceKernelGetGenuineProsperoSystemSwVersion]

- [sceKernelGetGenuineProsperoSystemSwVersion](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) (libkernel_sys) forwards to `sceKernelGetProsperoSystemSwVersion` and reports the configured guest firmware version, defaulting to `01.000.000` (`0x01000000`).
