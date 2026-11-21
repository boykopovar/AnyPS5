# libc

## add Unknown function info

- [filesystem _Stat and _Lstat type outputs](../../../core/libs/prx/libc/src/VersionedFilesystem.cpp) (libc) - the [`std::__sce_v2::filesystem`](../../../core/libs/prx/libc/src/VersionedFilesystemCalls.cpp) and [unversioned](../../../core/libs/prx/libc/include/GuestFilesystemPrimitives.hpp) status-type helpers write the permissions and the error with `GuestCopyOut_nid_no_patch`; these calls return the file type and have no other error channel, so an unwritable output is skipped and the type is still returned, which is a host-safety fallback; whether the console module faults on unmapped memory is unmeasured.
