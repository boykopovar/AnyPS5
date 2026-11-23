# libkernel

## add Unknown function info

- [pthread_attr_getstackaddr and scePthreadAttrGetstack](../../../core/libs/prx/libkernel/Pthread/Posix/Attr.cpp) (libkernel) - safely probe guest attribute handles and output pointers through LoadAttribute and StoreOutput (GuestCopyIn_nid_no_patch / GuestCopyOut_nid_no_patch), returning PosixThread::GUEST_EFAULT (14) or SCE_KERNEL_ERROR_EFAULT (0x8002000e) for unmapped or unwritable memory and EINVAL on null output pointers while preserving caller errno, instead of faulting the host. Returning EFAULT on bad guest memory is a host-safety fallback; retail console behavior has not been measured.
