# libSceShellUIUtil

## change [sceShellUIUtilLaunchByUri and sceShellUIUtilLaunchTask pointer safety]

- [sceShellUIUtilLaunchByUri, sceShellUIUtilLaunchTask and sceShellUIUtilGetAppUrl pointer safety](../../../core/libs/prx/libSceShellUIUtil/Export.cpp): unreadable URI and launch parameter pointers in `sceShellUIUtilLaunchByUri`, unreadable URI and unwritable output buffer in `sceShellUIUtilLaunchTask`, and unreadable string, unwritable output buffer (when capacity is non-zero) and unwritable output size pointers in `sceShellUIUtilGetAppUrl` return `0x80BA0020` (`ErrorInvalidArgument`) with `errno` preserved.
