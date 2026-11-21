# libkernel_sys

## change [Nine unnamed additional exports]

- [Nine unnamed additional exports](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) (libkernel_sys, NIDs `pCbBddNl66Y`, `zAloppPJxy0`, `vq2+HyP5U5I`, `z+e0vV-tnH4`, `gJ5gpW45mU0`, `N94a0T4F4+U`, `C49jelxiaVE`, `1WXQlxMpnNA`, `FR0odl35yQY`) - return what firmware 13.42's `system/common/lib/libkernel_sys.sprx` returns, read from disassembly to the return. The first five are `xor %eax,%eax; ret`, `N94a0T4F4+U` is `mov $0x8002004e,%eax; ret` and `C49jelxiaVE` is `mov $0x20018,%eax; ret`. `1WXQlxMpnNA` stores 1 to a zero-initialised module byte and returns 0, and `FR0odl35yQY` returns that byte zero-extended; nothing else touches the byte. None reads an argument, so the declarations are parameterless; the 32-bit loads clear the upper half of `rax`, so they return `std::uint64_t`. Names and callers are unknown. A PS5 Pro payload process (13.60) resolved all nine by NID text, answering these exact values; `FR0odl35yQY` read 0 before the first `1WXQlxMpnNA` call and 1 after.

## change [sceKernelGetCpumodeGame]

- [sceKernelGetCpumodeGame](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - returning `SCE_KERNEL_ERROR_EFAULT` (`0x8002000E`) for an unwritable mode pointer is a host-safety fallback; firmware 13.42 disassembly stores the result directly (`16a8c: mov %eax,(%r14)`), so the console faults. The success path was measured in a payload process on a PS5 Pro (firmware 13.60): return 0 with mode 0 written

## change [sceKernelIsNeoMode]

- [sceKernelIsNeoMode](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - the PS5 module's body is not read; measured in a payload process on a PS5 Pro (firmware 13.60, no sfo and no param.json), which answered 1, as did `sceKernelHasNeoMode` and `sceKernelIsAuthenticNeo`, both returning 1 here too. A title with `param.json` throws: what a PS5 application gets is unmeasured. For a title with `param.sfo` it returns bit 23 (0x00800000) of `ATTRIBUTE`, the title's Neo-mode support, and 0 without the key, as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/3eecea94ddaca360b19f4b800523b609a8b2ffcc/src/core/libraries/kernel/process.cpp#L29-L35) answers on a Neo console ([bit 23](https://github.com/shadps4-emu/shadPS4/blob/3eecea94ddaca360b19f4b800523b609a8b2ffcc/src/common/elf_info.h#L50-L51)). A base PS4 (firmware 10.71) answers 0 ([discussions/7941](https://github.com/boykopovar/AnyPS5/discussions/7941))
