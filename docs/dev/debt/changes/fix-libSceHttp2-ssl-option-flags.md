# libSceHttp2

## add Functional

- [sceHttp2SslEnableOption, sceHttp2SslDisableOption flags](../../../core/libs/prx/libSceHttp2/Export.cpp) (libSceHttp2) - flags 0x2, 0x80, 0x2000 and, for enable only, 0x100 throw: firmware 13.60's `libSceHttp2.sprx` accepts them (enable at 0xa90 rejects only `0xffffde40` @ 0xb26, disable at 0xbb0 only `0xffffdf40` @ 0xc56), but what the SSL transport does with them is not modelled. Those rejected bits return `0x817b11fe` before the id is looked up, as there. Disable also asks a system-state check (0x1a7f0) whether bits 0x25 may be cleared, and refuses with `0x817b5224` when the stored flags have 0x100; here both are always allowed
