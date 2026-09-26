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
- The shader recompiler [skips baryctric coordinates](../core/shader/recompiler/Recompiler.cpp) (is not even passed to SpirvTargetOptions at row 219).

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

- There's no way to specify keyboard and mouse input mapping when using a gamepad. The [default mapping](../core/libs/prx/libScePad/include/InputMapping.hpp) is always used.
- [Shader recompilation](../core/shader/recompiler/Recompiler.cpp) currently occurs right before it was transferred to Vulkan with caching, but should be moved to the [relinker](../core/relinker/main.cpp) stage. For this purpose, [shader/recompiler](../core/shader/recompiler) was written completely independently from [libs/prx](../core/libs/prx).
- The executable file that [relinker](../core/relinker/elfpatcher/src/windows/WindowsPeWriter.cpp) generates opens the console when launched, which is inconvenient for playability.
- [Relinker](../core/relinker/elfpatcher/src) doesn't add an icon to the generated executable. This should be done without adding dependencies (only standard).
- The game can expect its modified prx from the `sce_module`/`sce_modules` folder - this support is not implemented.
