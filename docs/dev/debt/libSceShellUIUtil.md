# libSceShellUIUtil technical debt

### Functional

- [sceShellUIUtilLaunchByUri](../../../core/libs/prx/libSceShellUIUtil/Export.cpp), [sceShellUIUtilGetAppUrl](../../../core/libs/prx/libSceShellUIUtil/Export.cpp), and [sceShellUIUtilLaunchTask](../../../core/libs/prx/libSceShellUIUtil/Export.cpp) validate basic parameter structures and return `0x80B8000A` (`SHELLUIUTIL_ERROR_INVALID_ARGUMENT`) when passed null pointers, undersized parameter blocks (`size < 8`), zero capacity, or negative task tokens. Valid arguments proceed to `NotImplemented_nid_no_patch` as background IPC dispatch and shell services remain unimplemented.

### Unknown function info

- [libSceShellUIUtil](../../../core/libs/prx/libSceShellUIUtil/Export.cpp) remains unimplemented. Native wrappers confirm the export signatures, including two input strings, output buffer, 32-bit capacity and optional 32-bit output size for `GetAppUrl`. The precise string meanings and the second word of the 8-byte `LaunchByUri` parameter require the receiving IPC service; the wrapper only checks its leading size. Task completion, IPC errors and lifecycle behavior are not implemented.

- [sceShellUIUtilAbortTask](../../../core/libs/prx/libSceShellUIUtil/Export.cpp) throws through `NotImplemented_nid_no_patch`. Its name and NID `20O48GLcA-w` are bound by the [13.60 firmware inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceshelluiutil-8fc7bce6b8.jsonl); its signature and behaviour remain unknown. The parameterless `int` declaration is an import-resolution placeholder, not a confirmed ABI. Task cancellation and its errors are unimplemented.
