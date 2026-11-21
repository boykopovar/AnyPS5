# libSceLibcInternal

## add Functional

- [_Mtxinit](../../../core/libs/prx/libSceLibcInternal/Mutex.cpp) (libSceLibcInternal) copies at most the 32 bytes of the name it labels the mutex with, through `CopyGuestString_nid_no_patch` or one `GuestCopyIn_nid_no_patch`, so an unreadable name returns 1 before any mutex is made. This is a host-safety fallback: the console's result for an inaccessible name is not measured. Closes when: compared with the console.
