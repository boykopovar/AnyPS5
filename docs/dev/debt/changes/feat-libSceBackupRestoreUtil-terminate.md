# libSceBackupRestoreUtil

## change [The uninitialized answers]

- [The uninitialized answers](../../../core/libs/prx/libSceBackupRestoreUtil/Export.cpp) (libSceBackupRestoreUtil) - 19 backup, restore, progress, information, size, title, delete and cancel exports return `0x81300000`, and `sceBackupRestoreUtilTerminate` returns 0, only the 32-bit result being claimed, because `sceBackupRestoreUtilInitialize` throws and the library is never initialized here. In firmware 13.42's `libSceBackupRestoreUtil.sprx` (SHA-256 `7bd336b468f2ace0b5e9959535ff79543a3b68bc6e0d5738c2b00445a1e71aec`), read from disassembly and not run on a console, each loads the pointer at `0xc018` first and returns `0x81300000` when it is null. It lies beyond its segment's file data, so it starts null; `sceBackupRestoreUtilInitialize` passes its address on at `0xb5c` and `sceBackupRestoreUtilTerminate` clears it. Names, signatures and the error's name are unverified.
