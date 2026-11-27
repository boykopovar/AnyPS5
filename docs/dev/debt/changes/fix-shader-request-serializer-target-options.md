# shader

## remove Captured shader target robustness

## change Shader request captures
- [Shader request captures](../../../core/shader/recompiler/ControlFlow/src/RequestSerializer.cpp) written by versions before 64 omit the target options `pixelWave32Subgroups`, `robustBufferAccess2`, `exactRobustLoads`, `waveLdsMemoryBarriers`, `deviceScopeLdsMemoryBarriers`, `ldsLockFences`, `deviceAtomicWorkgroupMemoryBarriers`, `meshWindowBuffer`, `float16HostNans`, `meshInlineFunctions` and `spannedShuffleThroughMemory`. Replaying one restores their defaults, which can change memory guarding or synchronization; a successful replay of such a capture does not establish equivalence to the capture device's shader. Versions before 57 also omit `maxImageArrayLayers` and `fixedPushSlots`, and a capture cannot recover either one.