# libkernel

## add Unknown function info

- [scePthreadAttrGetaffinity, scePthreadAttrGetsolosched and scePthreadAttrSetaffinity](../../../core/libs/prx/libkernel/Pthread/src/Attr.cpp) (libkernel) - safely probe guest attribute handles and output pointers through LoadAttribute and StoreOutput (GuestCopyIn_nid_no_patch / GuestCopyOut_nid_no_patch), returning SCE_KERNEL_ERROR_EFAULT (0x8002000e) for unmapped or unwritable memory and SCE_KERNEL_ERROR_EINVAL on null output pointers instead of faulting the host or throwing runtime errors. Returning EFAULT on bad guest memory is a host-safety fallback; retail console behavior has not been measured.
