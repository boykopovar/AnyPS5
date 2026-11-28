# libkernel_sys

## add Functional

- [kmq_open and kmq_setattr guest attribute pointers](../../../core/libs/prx/libkernel_sys/Mqueue.cpp) (libkernel_sys) - safely probe guest attribute pointers in `kmq_open` (when `O_CREAT` is set) and `kmq_setattr` via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch`, returning -1 with `EFAULT` (14) while preserving caller `errno` for unmapped or unwritable memory as a host-safety fallback; retail console fault behavior has not been measured.
