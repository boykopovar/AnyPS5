# libScePosixForWebKit

## add Functional

- [freeaddrinfo_nid_postfix](../../../../core/libs/prx/libScePosixForWebKit/Resolver.cpp) (libScePosixForWebKit) - probing each `GuestResolver::AddressInfo` node through `GuestCopyIn_nid_no_patch`, stopping cleanly on unreadable pointers, and preserving `errno` is a host-safety fallback; console behaviour on invalid `freeaddrinfo` pointers is unmeasured.
