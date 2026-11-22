# libc

## add Unknown function info

- [std::__sce_v2::filesystem _Last_write_time and _File_size outputs](../../../core/libs/prx/libc/src/VersionedFilesystemCalls.cpp) (libc) - the write time and the size error are written with `GuestCopyOut_nid_no_patch`; an unwritable time output returns 14 (EFAULT) in the same error-number channel as the stat failures, and an unwritable error output is skipped while the size result stays `UINT64_MAX`. These are host-safety fallbacks; whether the console module faults on unmapped memory is unmeasured.

# libSceLibcInternal

## add Unknown function info

- [std::filesystem _Last_write_time and _File_size outputs](../../../core/libs/prx/libSceLibcInternal/Filesystem.cpp) (libSceLibcInternal) - the write time and the size error are written with `GuestCopyOut_nid_no_patch`; an unwritable time output returns 14 (EFAULT) in the same error-number channel as the stat failures, and an unwritable error output is skipped while the size result stays `UINT64_MAX`. These are host-safety fallbacks; whether the console module faults on unmapped memory is unmeasured.
