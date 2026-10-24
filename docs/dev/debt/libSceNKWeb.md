# libSceNKWeb technical debt

### Functional

- [sceNKWebInitialize and sceNKWebTerminate](../../../core/libs/prx/libSceNKWeb/Export.cpp) remain throwing stubs. Both native exports take no input arguments and return a 32-bit status; decompiler SIMD parameters are unused register propagation through initialization and cleanup helpers.
