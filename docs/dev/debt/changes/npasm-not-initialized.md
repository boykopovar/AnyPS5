# libSceNpManager

## add Unknown function info

- [sceNpAsm and sceNpAsmClient calls before initialization](../../../core/libs/prx/libSceNpManager/Export.cpp) (libSceNpManager) - 29 `sceNpAsmClient` exports return `SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED` (`0x8055A282`) while the client is not initialized, and 7 `sceNpAsm` exports return `SCE_NP_ASM_ERROR_NOT_INITIALIZED` (`0x8055A202`) while the service is not, whatever their arguments; otherwise they still throw. Firmware 13.42's `libSceNpManager.sprx` tests the byte `0x62f60` (set by `sceNpAsmClientInitialize`, cleared by `sceNpAsmClientTerminate`) or `0x631bc` (`sceNpAsmInitialize`, `sceNpAsmTerminate`) first; read from its disassembly and run under an emulator, not measured on a console. What they do once initialized, and their signatures, are unknown
