# libScePosixForWebKit

## add Functional

- [freeaddrinfo](../../../core/libs/prx/libScePosixForWebKit/Resolver.cpp) (libScePosixForWebKit) - safely probes each guest `GuestResolver::AddressInfo` node via `GuestCopyIn_nid_no_patch` and stops on null or unmapped pointers while preserving caller `errno`, instead of dereferencing guest pointers directly. Ignoring unmapped pointers is a host-safety fallback; retail console fault behavior has not been measured.
