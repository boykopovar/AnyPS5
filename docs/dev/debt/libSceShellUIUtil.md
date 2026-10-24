# libSceShellUIUtil technical debt

### Unknown function info

- [libSceShellUIUtil](../../../core/libs/prx/libSceShellUIUtil/Export.cpp) remains unimplemented. Native wrappers confirm the export signatures, including two input strings, output buffer, 32-bit capacity and optional 32-bit output size for `GetAppUrl`. The precise string meanings and the second word of the 8-byte `LaunchByUri` parameter require the receiving IPC service; the wrapper only checks its leading size. Task completion, IPC errors and lifecycle behavior are not implemented.
