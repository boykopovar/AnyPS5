# libkernel

## change [sceKernelTitleWorkaroundIsEnabled]

- [sceKernelTitleWorkaroundIsEnabled](../../../core/libs/prx/libkernel/SysExports.cpp) (libkernel and libkernel_sys) - bits above 0x78 return `EINVAL`: a PS5 payload process (firmware 11.60) accepted 0 to 0x78 and refused 0x79 to 0x7F, while firmware 13.42 disassembly accepts 0 to 0x7F and 4.03's `libkernel.sprx` bounds it at 0x57 (RVA `0x20500`); the bit is compared unsigned. Misspelled `sceKernelTitleWorkdaroundIsEnabled` runs it. Titles usually pass the `title_workaround` block of `sceKernelGetAppInfo`, whose `{version, align, ids[2]}` bytes are written as zeros here (its offset in the 0x58-byte structure is unmeasured), so its bits read 0; an unreadable input or unwritable output returns `SCE_KERNEL_ERROR_EFAULT` (0x8002000e) as a host-safety fallback, unmeasured on a console

# libkernel_sys

## change [sceKernelTitleWorkaroundIsEnabled2]

- [sceKernelTitleWorkaroundIsEnabled2](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - what the 64-bit second argument means is unknown: PS5 firmware 13.42 `system/common/lib/libkernel_sys.sprx` (read from disassembly to the return, not measured on a console) only refuses a value of 0x29 or more, unsigned, with `SCE_KERNEL_ERROR_EINVAL` (`0x80020016`) and does not otherwise read it. Its other checks and the bit test are those of `sceKernelTitleWorkaroundIsEnabled`, with a null workaround or output pointer checked first (`0x8002000e`), and it uses the same host-safety fallback for unreadable or unwritable pointers. Its bit limit is 0x78 as for `sceKernelTitleWorkaroundIsEnabled`: a PS5 payload process (firmware 11.60) accepted bits 0 to 0x78 and refused 0x79 to 0x7F for selectors 0 and 0x28, with the error code not printed, so `EINVAL` here follows the 13.42 reading
