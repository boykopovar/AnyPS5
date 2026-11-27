# libSceNKWebKit

## add Functional

- [WebCore::ResourceRequestBase guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/ResourceRequest.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::ResourceRequestBase` exports is unmeasured; exports preserve null-argument `std::invalid_argument` checks, validate required guest reads (`GuestArenaPrepareHostRead_nid_no_patch` + `GuestReadable_nid_no_patch`) before `GuestCopyIn_nid_no_patch`, guard guest writes with active `GuestArena::HostWrite` scopes and `GuestWritable_nid_no_patch`, and report failed required guest-memory accesses through `NotImplementedAt_nid_no_patch`. Closes when: console behavior for invalid guest pointers in `ResourceRequestBase` exports is verified on hardware.
