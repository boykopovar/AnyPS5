# libkernel_sys

## add Functional

- [kmq_timedreceive guest output](../../../core/libs/prx/libkernel_sys/Mqueue.cpp) (libkernel_sys) - returning 14 (EFAULT) with the caller's errno preserved for an unwritable message buffer or priority pointer via GuestCopyOut_nid_no_patch is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
