# libScePsm

## add Functional

- [ScePsmMonoArrayLength, ScePsmMonoGcWbarrierGenericStore, ScePsmMonoStringLength and ScePsmMonoStringToUtf8](../../../core/libs/prx/libScePsm/Export.cpp) (libScePsm) - read object length fields via `GuestCopyIn_nid_no_patch`, read UTF-16 string units in page-bounded (`0x1000`) chunks, and write write-barrier slots via `GuestCopyOut_nid_no_patch` while preserving `errno`, throwing `std::invalid_argument` on null or inaccessible guest pointers instead of faulting; that exception path is a host-safety fallback and has not been measured on a console.
