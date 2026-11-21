# libSceNKWebKit

## add Functional

- [WTF::memoryFootprint](../../../core/libs/prx/libSceNKWebKit/src/WtfMemoryFootprint.cpp) - Console build flags, ABI and measurements are unverified. WebKitFeatures.cmake defaults x86-64 USE_SYSTEM_MALLOC off; PlatformPlayStation.cmake selects MemoryFootprintGeneric.cpp, reaching FreeBSD ki_rssize * vmPageSize() in [AvailableMemory.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/bmalloc/bmalloc/AvailableMemory.cpp). Host RSS includes the process and its libraries: Windows working-set size, Linux statm resident pages, macOS task_info resident size. Linux counters can lag; macOS is untested. Failed queries throw instead of the upstream FreeBSD zero result; unsupported hosts throw NotImplemented_nid_no_patch.

## remove | `_ZN3WTF15memoryFootprintEv`
