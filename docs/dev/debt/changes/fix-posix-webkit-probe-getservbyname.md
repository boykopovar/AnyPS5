# libScePosixForWebKit

## add Unknown function info

- [getservbyname name and protocol](../../../core/libs/prx/libScePosixForWebKit/Resolver.cpp) (libScePosixForWebKit) - the service name and protocol are copied with `CopyGuestString_nid_no_patch` capped at 64 bytes, well past the longest entry in the service table, so longer names still match nothing; an unreadable name or protocol returns null like an unknown service, because the console's behaviour on unmapped memory is unmeasured.
