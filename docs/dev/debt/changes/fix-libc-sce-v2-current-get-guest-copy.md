# libc

## add Functional

- [std::__sce_v2::filesystem::_Current_get](../../../core/libs/prx/libc/src/VersionedFilesystem.cpp) (libc) writes the sandbox root `/` and its terminator with one `GuestCopyOut_nid_no_patch` and returns false when the buffer can't be written, keeping `errno`. This is a host-safety fallback; the console's behaviour for an inaccessible buffer is not measured.
