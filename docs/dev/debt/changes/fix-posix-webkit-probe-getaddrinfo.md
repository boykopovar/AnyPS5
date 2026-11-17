# libScePosixForWebKit

## add Unknown function info

- [getaddrinfo hints and result pointer](../../../core/libs/prx/libScePosixForWebKit/Resolver.cpp) (libScePosixForWebKit) - the hints are copied in with `GuestCopyIn_nid_no_patch` and the result pointer is cleared and finally stored with `GuestCopyOut_nid_no_patch`; unreadable hints or an unwritable result pointer return `EAI_FAIL` (4) like a null result pointer, and a list built before a failed final store is freed, because the console's behaviour on unmapped memory is unmeasured. The node and service strings are still read in place.
