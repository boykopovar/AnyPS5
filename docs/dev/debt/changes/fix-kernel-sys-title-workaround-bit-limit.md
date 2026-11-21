# libkernel

## change [sceKernelTitleWorkaroundIsEnabled]

- [sceKernelTitleWorkaroundIsEnabled](../../../core/libs/prx/libkernel/SysExports.cpp) (libkernel and libkernel_sys) - AnyPS5 caps the unsigned bit at 0x78. An [11.60 PS5 payload](https://github.com/boykopovar/AnyPS5/pull/22365) accepted 0 through 0x78 and refused 0x79 through 0x7f (`EINVAL` at 0x7f). A [13.60 PS5 Pro payload](https://github.com/boykopovar/AnyPS5/pull/24656#issuecomment-7054016081) accepted sampled bits through 0x7f. The unsigned bound in disassembly is 0x57 in 4.03 `libkernel.sprx` (RVA `0x20500`) and 0x7f in 13.42 `libkernel` and `libkernel_sys`. `sceKernelTitleWorkdaroundIsEnabled` is an alias. `sceKernelGetAppInfo` returns zeros at shadPS4's offsets for `{version, align, ids[2]}` (0x30 to 0x48). The actual offset in the console's 0x58-byte app-info structure remains unmeasured. Unreadable input or unwritable output returns `SCE_KERNEL_ERROR_EFAULT` (`0x8002000e`) as a host fallback. Console behavior for unwritable memory is unmeasured.

# libkernel_sys

## change [sceKernelTitleWorkaroundIsEnabled2]

- [sceKernelTitleWorkaroundIsEnabled2](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - what the 64-bit second argument means is unknown: PS5 firmware 13.42 `system/common/lib/libkernel_sys.sprx` (read from disassembly to the return, not measured on a console) only refuses a value of 0x29 or more, unsigned, with `SCE_KERNEL_ERROR_EINVAL` (`0x80020016`) and does not otherwise read it. Its other checks and the bit test are those of `sceKernelTitleWorkaroundIsEnabled`, with a null workaround or output pointer checked first (`0x8002000e`), and it uses the same host-safety fallback for unreadable or unwritable pointers. Its bit limit is 0x78 as for `sceKernelTitleWorkaroundIsEnabled`: a PS5 payload process (firmware 11.60) accepted bits 0 to 0x78 and refused 0x79 to 0x7F for selectors 0 and 0x28, with the error code not printed, so `EINVAL` here follows the 13.42 reading
