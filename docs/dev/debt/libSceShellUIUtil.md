# libSceShellUIUtil technical debt

### Unknown function info

- [libSceShellUIUtil](../../../core/libs/prx/libSceShellUIUtil/Export.cpp): `sceShellUIUtilAbortTask`, `sceShellUIUtilGetAppUrl`, `sceShellUIUtilLaunchByUri` and `sceShellUIUtilLaunchTask` throw through `NotImplemented_nid_no_patch` once their argument checks pass and the library is initialized. The 13.42 module (sha256 7dfd650d22687a8ab54632cf8f9cdce2bd7637950b19c7005606debeb0a668f9) then connects an IPMI client and calls the shell service, whose replies, task completion and errors are not modelled. The meanings of the strings and of the second word of the 8-byte `LaunchByUri` parameter belong to that service; the module only checks the parameter's leading size. Checks read from disassembly, not measured on a console.

### Functional

- [sceShellUIUtilLaunchByUri and sceShellUIUtilLaunchTask pointer safety](../../../core/libs/prx/libSceShellUIUtil/Export.cpp): unreadable launch parameter in `sceShellUIUtilLaunchByUri` and unwritable output buffer in `sceShellUIUtilLaunchTask` return `0x80BA0020` (`ErrorInvalidArgument`) with `errno` preserved.
