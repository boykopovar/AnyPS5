# libSceVideoOut

## change [sceVideoOutRegisterBuffers, sceVideoOutGetFlipStatus, sceVideoOutGetVblankStatus]

- [sceVideoOutRegisterBuffers, sceVideoOutGetFlipStatus, sceVideoOutGetVblankStatus](../../../core/libs/prx/libSceVideoOut/src/Buffer.cpp) (libSceVideoOut) - PS4 titles use the 64-byte flip and 32-byte vblank layouts before buffer registration based on the process title ID. The pre-registration layouts are unmeasured on a PS4 title. Registering PS4 buffers also selects these layouts. The PS4 timing fields `tsc` and `submitTsc` are zero. PS4 registration accepts A8R8G8B8_SRGB (0x80000000), A8B8G8R8_SRGB (0x80002200) and 0x88000000 buffers with option 0 or 8 (next entry). For `DisplayTilingGcn`, see PS4 tiling and CPU scanout in [libSceAgcDriver.md](libSceAgcDriver.md). The PS4 linear alignment rule is unknown. The OpenOrbis 1920x1080 sample places the second buffer 8,294,400 bytes after the first, consistent with 4 KiB alignment
