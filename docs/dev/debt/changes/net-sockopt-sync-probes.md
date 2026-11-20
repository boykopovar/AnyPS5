# libSceNet

## add Unknown function info

- [sceNetGetsockopt, sceNetSetsockopt, sceNetSyncSignal, sceNetSyncWait, libSceNetUnknown_DnB6WJ91HGg](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - safely probe and copy guest memory for socket option parameters, sync primitives, resolver connection flags and DHCP option buffers using GuestCopyIn and GuestCopyOut, returning EFAULT or invalid error when guest buffers are unreadable or unwritable instead of dereferencing guest pointers.
