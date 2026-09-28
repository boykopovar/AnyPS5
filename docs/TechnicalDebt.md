# Project technical debt

### Build

- Building on Windows requires a specific version of mingw - MinGW-w64 GCC 15.2.0 (`winlibs-gcc15`, `x86_64-ucrt-posix-seh`)
- Even compiled prx libraries on Windows require nearby (static linking of these dependencies causes conflicts):
  - libgcc_s_seh-1.dll
  - libstdc++-6.dll
  - libwinpthread-1.dll

### Silent stubs

Throughout the project, every function at every stage either **does exactly what it's supposed to or throws an exception**. Everywhere... except:
- [libSceSaveDataDialog.native](../core/libs/prx/libSceSaveDataDialog.native/Export.cpp)
- [libSceCommonDialog](../core/libs/prx/libSceCommonDialog/Export.cpp)
- The shader recompiler [skips baryctric coordinates](../core/shader/recompiler/Recompiler.cpp) (is not even passed to SpirvTargetOptions at row 212).

### Unknown function info

- [zARR5aCmkoY](../core/libs/prx/libSceAgc/DcbFlow/src/Control.cpp) (libSceAgc) - unknown name, signature
- [qj7QZpgr9Uw](../core/libs/prx/libSceAgc/DcbState/src/ContextState.cpp) (libSceAgc) - unknown name
- [fd5Bp5tGTgo](../core/libs/prx/libSceAgc/Misc/src/ShaderFusion.cpp) (libSceAgc) - unknown name
- [dolOmWH+huQ](../core/libs/prx/libSceAgc/Misc/src/ShaderFusion.cpp) (libSceAgc) - unknown name
- [V++UgBtQhn0](../core/libs/prx/libSceAgc/Misc/src/PacketInfo.cpp) (libSceAgc) - unknown name
- [gQkqkLttcpw](../core/libs/prx/libSceAgc/Acb/src/Control.cpp) (libSceAgc) - unknown name, signature
- [sceKernelInternalMemoryGetModuleSegmentInfo](../core/libs/prx/libkernel/Module/src/Module.cpp) (libkernel) - unknown signature
- [sceLibcInternalBacktraceForGame](../core/libs/prx/libc/src/HeapDiagnostics.cpp) (libSceLibcInternal, implemented in libc) - unknown signature
- [sceLibcInternalHeapErrorReportForGame](../core/libs/prx/libc/src/HeapDiagnostics.cpp) (libSceLibcInternal, implemented in libc) - unknown signature
- [__progname](../core/libs/prx/libkernel/System/src/Process.cpp) (libkernel) - unknown data export

### Functional

- [Shader recompilation](../core/shader/recompiler/Recompiler.cpp) currently occurs right before it was transferred to Vulkan with caching, but should be moved to the [relinker](../core/relinker/main.cpp) stage. For this purpose, [shader/recompiler](../core/shader/recompiler) was written completely independently from [libs/prx](../core/libs/prx).
- The executable file that [relinker](../core/relinker/elfpatcher/src/windows/WindowsPeWriter.cpp) generates opens the console when launched, which is inconvenient for playability.
- [Relinker](../core/relinker/elfpatcher/src) doesn't add an icon to the generated executable. This should be done without adding dependencies (only standard).
- `--to-intel` does not lower the register form of INSERTQ (`F2 0F 79`) nor MONITORX/MWAITX/CLZERO/RDPRU/MCOMMIT; the [lowering](../core/relinker/codegen/src/x86/Sse4aLowering.cpp) fails the relink instead. SHA-NI is not substituted.
- The length-changing path of the [instruction rewriter](../core/relinker/codegen/src/x86/X64InstructionRewriter.cpp) is not used by the [converter](../core/relinker/codegen/src/Amd64OnlyConverter.cpp): it does not adjust VEX/0F38/0F3A RIP-relative operands, data-to-code references (relocations, FDEs, jump tables) or segment sizes, so every substitution keeps the instruction length.
- The Linux placement of the `--to-intel` stubs in the [ELF patcher](../core/relinker/elfpatcher/src/linux/LinuxElfPatcher.cpp) is covered only by a synthetic test.
- The libc [SSE4a trap emulation](../core/libs/prx/libc/src/specifics/windows/Sse4aEmulation.hpp) on Windows is superseded by `--to-intel` and remains only until the relinked title has been verified without it.
- `--to-intel` lowerings that need a stub section are rejected for `sce_module` guest modules: the [guest module builder](../core/relinker/relinker/src/guest/GuestModuleBuilder.cpp) does not pass stub sites to the [guest module writer](../core/relinker/elfpatcher/include/elfpatcher/general/GuestModuleWriter.hpp).
- The game can expect its modified prx from the `sce_module`/`sce_modules` folder - this support is not implemented.
