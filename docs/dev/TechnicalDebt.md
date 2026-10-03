# Project technical debt

### Build

- Building on Windows requires a specific version of mingw - MinGW-w64 GCC 15.2.0 (`winlibs-gcc15`, `x86_64-ucrt-posix-seh`)
- Even compiled prx libraries on Windows require nearby (static linking of these dependencies causes conflicts):
  - libgcc_s_seh-1.dll
  - libstdc++-6.dll
  - libwinpthread-1.dll

### Silent stubs

Throughout the project, every function at every stage either **does exactly what it's supposed to or throws an exception**. Everywhere... except:
- [libSceSaveDataDialog.native](../../core/libs/prx/libSceSaveDataDialog.native/Export.cpp)
- [libSceCommonDialog](../../core/libs/prx/libSceCommonDialog/Export.cpp)
- [libSceHmd](../../core/libs/prx/libSceHmd/Export.cpp) implements only the disconnected-headset path: initialization succeeds, device queries report `NotDetected`, and opening a device returns `DeviceDisconnected`. Headset support, tracking, rendering and additional HMD exports are not implemented; SDK-level ABI compatibility and in-game behaviour remain unverified.
- [libSceNpCommerce](../../core/libs/prx/libSceNpCommerce/Export.cpp) - the PS Store icon show/hide calls do nothing
- [sceVideoOutOpen](../../core/libs/prx/libSceVideoOut/src/Output.cpp) validates the priority and CPU affinity that the open param requests for the VideoOut service thread but does not apply them: the port's present and vblank threads are host threads, and guest priorities and affinities do not reach host scheduling
- The shader recompiler [skips baryctric coordinates](../../core/shader/recompiler/Recompiler.cpp) (is not even passed to SpirvTargetOptions at row 212).
- The shader recompiler [ignores `s_round_mode`](../../core/shader/recompiler/Translation/src/ScalarInstructions.cpp): float operations keep rounding to nearest even after it selects another rounding mode.
- [libSceAvPlayer](../../core/libs/prx/libSceAvPlayer/Export.cpp): `sceAvPlayerSetLogCallback` accepts a callback that is never called, as the player produces no log messages, and `sceAvPlayerSetAvailableBandwidth` has no effect, as it governs HLS sources, which `sceAvPlayerAddSource` does not implement.
- [sceAvPlayerSetTrickSpeed](../../core/libs/prx/libSceAvPlayer/src/Source.cpp) (libSceAvPlayer) with a negative speed runs the clock backwards but delivers no frames; when a forward speed is set again, playback resumes from the rewound time.

### Unknown function info

- [sceVideoOutOpen](../../core/libs/prx/libSceVideoOut/src/Output.cpp) (libSceVideoOut) - the open param's first word is unknown (PPSA21564 passes 16; it is not the byte size, since the affinity mask is at offset 16); only 16 is accepted. Whether the param continues past offset 24 is also unknown
- [AMPR WaitOnAddress / WaitOnCounter](../../core/libs/prx/libkernel/Apr/src/Apr.cpp) (libSceAmpr) - compare encoding assumed to be the WAIT_REG_MEM one (0 always, 1 <, 2 <=, 3 ==, 4 !=, 5 >=, 6 >)
- [sceAgcSetSubmitMode](../../core/libs/prx/libSceAgc/Misc/src/Suspend.cpp) (libSceAgc) - mode values unknown; only 0 is accepted
- [zARR5aCmkoY](../../core/libs/prx/libSceAgc/DcbFlow/src/Control.cpp) (libSceAgc) - unknown name, signature
- [qj7QZpgr9Uw](../../core/libs/prx/libSceAgc/DcbState/src/ContextState.cpp) (libSceAgc) - unknown name
- [fd5Bp5tGTgo](../../core/libs/prx/libSceAgc/Misc/src/ShaderFusion.cpp) (libSceAgc) - unknown name
- [dolOmWH+huQ](../../core/libs/prx/libSceAgc/Misc/src/ShaderFusion.cpp) (libSceAgc) - unknown name
- [V++UgBtQhn0](../../core/libs/prx/libSceAgc/Misc/src/PacketInfo.cpp) (libSceAgc) - unknown name
- [gQkqkLttcpw](../../core/libs/prx/libSceAgc/Acb/src/Control.cpp) (libSceAgc) - unknown name, signature
- [sceKernelInternalMemoryGetModuleSegmentInfo](../../core/libs/prx/libkernel/Module/src/Module.cpp) (libkernel) - unknown signature
- [sceKernelSyncOnAddressWait](../../core/libs/prx/libkernel/SyncOnAddress/src/SyncOnAddress.cpp) (libkernel) - the only known caller passes a null timeout and a name string as the fourth argument; the timeout is assumed to point to microseconds like the other kernel waits, and the name is ignored
- [sceKernelMapNamedFlexibleMemoryInternal](../../core/libs/prx/libkernel/DirectMemory/Export.cpp) (libkernel) - flag 0x8000 unknown; only the sceKernelMapNamedFlexibleMemory flags are accepted
- [sceLibcInternalBacktraceForGame](../../core/libs/prx/libc/src/HeapDiagnostics.cpp) (libSceLibcInternal, implemented in libc) - unknown signature
- [sceLibcInternalHeapErrorReportForGame](../../core/libs/prx/libc/src/HeapDiagnostics.cpp) (libSceLibcInternal, implemented in libc) - unknown signature
- [__progname](../../core/libs/prx/libkernel/System/src/Process.cpp) (libkernel) - unknown data export
- [pthread_barrierattr_setpshared](../../core/libs/prx/libkernel/Pthread/Posix/Barrier.cpp) (libkernel) - PTHREAD_PROCESS_SHARED throws: FreeBSD 9.0 libthr rejects it with EINVAL and 11.0 accepts it, and which one the console follows is unknown
- [sceSslClose](../../core/libs/prx/libSceSsl/Export.cpp) (libSceSsl) - unknown signature
- [sceSslGetSerialNumber](../../core/libs/prx/libSceSsl/Export.cpp) (libSceSsl) - unknown signature
- [X+4jdIS75P0](../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown name, signature
- [AOWqIYsgVHs](../../core/libs/prx/libSceContentExport/Export.cpp) (libSceContentExport) - unknown name, signature
- [GQTObcITIXI](../../core/libs/prx/libSceShare/Export.cpp) (libSceShare) - unknown name, signature
- [BnMAMrsfVWo](../../core/libs/prx/libc/src/HeapDiagnostics.cpp) (libc) - unknown name, signature
- AudioPropagation, AudioIn, NpSessionSignaling and PlayerInvitationDialog exports added without an implementation have assumed signatures
- [sceAvPlayerStartEx](../../core/libs/prx/libSceAvPlayer/Export.cpp) (libSceAvPlayer) - start info layout unknown; it is ignored and playback starts as with `sceAvPlayerStart`
- [sceAvPlayerInit / sceAvPlayerInitEx](../../core/libs/prx/libSceAvPlayer/src/Player.cpp) (libSceAvPlayer) - behaviour without a memory replacement unknown; frame and sample buffers then come from the guest heap
- [SceAvPlayerVideoEx](../../core/libs/SceTypes.hpp) (libSceAvPlayer) - frame rate field and encoding unknown; it is left zero in frame and stream info

### Functional

- [Shader recompilation](../../core/shader/recompiler/Recompiler.cpp) currently occurs right before it was transferred to Vulkan with caching, but should be moved to the [relinker](../../core/relinker/main.cpp) stage. For this purpose, [shader/recompiler](../../core/shader/recompiler) was written completely independently from [libs/prx](../../core/libs/prx).
- The executable file that [relinker](../../core/relinker/elfpatcher/src/windows/WindowsPeWriter.cpp) generates opens the console when launched, which is inconvenient for playability.
- [libSceJpegEnc](../../core/libs/prx/libSceJpegEnc/Export.cpp) encodes 4:2:2 sampling requests with 4:2:0 chroma subsampling, and grayscale requests as a 3-component JPEG with neutral chroma instead of a single-component one: the [JPEG encoder](../../core/Decoder/Jpeg/src/Jpeg.cpp) (stb) only produces 3-component 4:2:0 and 4:4:4 images.
- [Relinker](../../core/relinker/elfpatcher/src) doesn't add an icon to the generated executable. This should be done without adding dependencies (only standard).
- `--to-intel` does not lower RDPRU/MCOMMIT; the [matcher](../../core/relinker/codegen/src/x86/Amd64OnlyInstructionMatcher.cpp) fails the relink instead. The SHA-1 instructions of SHA-NI (SHA1RNDS4, SHA1NEXTE, SHA1MSG1, SHA1MSG2) are not substituted, and SHA-256 instructions with a memory operand fail the relink.
- The length-changing path of the [instruction rewriter](../../core/relinker/codegen/src/x86/X64InstructionRewriter.cpp) is not used by the [converter](../../core/relinker/codegen/src/Amd64OnlyConverter.cpp): it does not adjust VEX/0F38/0F3A RIP-relative operands, data-to-code references (relocations, FDEs, jump tables) or segment sizes, so every substitution keeps the instruction length.
- The Linux placement of the `--to-intel` stubs in the [ELF patcher](../../core/relinker/elfpatcher/src/linux/LinuxElfPatcher.cpp) is covered only by a synthetic test.
- The libc [SSE4a trap emulation](../../core/libs/prx/libc/src/specifics/windows/Sse4aEmulation.hpp) on Windows is superseded by `--to-intel` and remains only until the relinked title has been verified without it.
- `--to-intel` guest module trampolines are covered only by a synthetic relinker test; no game title has been verified with them on Linux or Windows.
- [sceKeyboardGetKey2Char](../../core/libs/prx/libSceKeyboard/src/keyboard_impl.cpp) (libSceKeyboard) translates only the 101-key (US) arrangement and throws for the 106-key (Japanese) one; Ctrl and Alt do not change the character.
- [libSceAudiodec](../../core/libs/prx/libSceAudiodec/Export.cpp) throws for the 24-bit PCM word size (`iBwPcm` 0): its sample layout is unknown. ATRAC9 decoding is covered only by configuration and error tests, as no ATRAC9 encoder is available for a fixture.
- [libScePngEnc](../../core/libs/prx/libScePngEnc/Export.cpp) honours `filter_type` only as all filters (adaptive) or a single filter: the [PNG encoder](../../core/Decoder/Png/src/Png.cpp) (stb) cannot restrict adaptive filtering to a subset, so the first filter in the mask is used.
- [libScePlayerInvitationDialog](../../core/libs/prx/libScePlayerInvitationDialog/libScePlayerInvitationDialog.cpp) simulates dialog completion without displaying UI or sending invitations; its parameter ABI remains unverified.
