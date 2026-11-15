# libSceNet

## add PS4 firmware 10.71 module

- [sceNetSyncCreate, sceNetSyncDestroy, sceNetSyncSignal, sceNetSyncWait](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - safely probe sync object and timeout pointers using `GuestWritable_nid_no_patch` and `GuestCopyIn_nid_no_patch`, returning `NET_EINVAL` for null pointers and `NET_EFAULT` for unmapped pointers while preserving guest errno.
