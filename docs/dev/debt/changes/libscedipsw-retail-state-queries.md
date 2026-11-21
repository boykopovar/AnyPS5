# libSceDipsw

## remove [sceKernelGetDiableBinaryVersionCheckValue]

## change [sceKernelCheckDipsw, sceKernelInitializeDipsw, sceKernelIsAssistMode, sceKernelIsDevelopmentMode, sceKernelIsDisableRazor, sceKernelReadDipswData, sceKernelSetDipsw, sceKernelUnsetDipsw and sceKernelWriteDipswData]

- [sceKernelReadDipswData and sceKernelWriteDipswData](../../../core/libs/prx/libSceDipsw/Export.cpp) (libSceDipsw) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceDipsw.c), but the buffer size semantics for the full switch block are not settled across the public scene baselines. The declarations remain parameterless import placeholders and the functions throw. `sceKernelInitializeDipsw`, `sceKernelIsAssistMode`, `sceKernelIsDevelopmentMode`, `sceKernelIsDisableRazor` and `sceKernelGetDiableBinaryVersionCheckValue` return the retail-unit values (0, 0, 0, 1, 0). `sceKernelCheckDipsw`, `sceKernelSetDipsw` and `sceKernelUnsetDipsw` take a `uint16_t` bit index as documented in the PS4 and PS5 scene baselines (shadPS4, Kyty, OpenOrbis) and operate on an in-memory 65536-bit switch block that starts cleared, so a retail check returns 0 until a title sets the same bit. This is a cross-SDK baseline, not a PS5 10.6 console measurement.
