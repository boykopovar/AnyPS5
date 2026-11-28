# libSceLibcInternal

## add Functional

- [inet_aton, inet_addr, and inet_network](../../../core/libs/prx/libSceLibcInternal/Inet.cpp) copy guest string inputs through `GuestCopyIn_nid_no_patch` and preserve `errno` around guest memory checks, failing through `NotImplemented_nid_no_patch(__func__)` on unreadable non-null string pointers.
