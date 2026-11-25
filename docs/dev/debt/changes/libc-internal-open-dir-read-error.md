# libSceLibcInternal

## add Functional

- [_Open_dir](../../../core/libs/prx/libSceLibcInternal/Filesystem.cpp) (libSceLibcInternal) - delegates to `GuestFilesystem::OpenDir`, which closes the directory handle, returns null, and writes the `readdir_r_nid_postfix` error code to `error` on an initial read failure (matching `libc` `VersionedFilesystem.cpp`). Not measured on a PS5.
