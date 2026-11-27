# libSceNKWebKit

## add Functional

- [CrossOriginRequest guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/CrossOriginRequest.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::isSimpleCrossOriginAccessRequest`, `WebCore::cleanHTTPRequestHeadersForAccessControl`, and `WebCore::updateRequestForAccessControl` is unmeasured; the exports copy guest strings, header vectors, `SecurityOrigin` fields, and `ResourceRequestBase` bitfield bytes through `GuestCopyIn_nid_no_patch`/`GuestCopyOut_nid_no_patch`, preflight writable `ResourceRequestBase` and common header buffers (including releasable string headers) under `GuestArena::HostWrite`, and call `NotImplementedAt_nid_no_patch` on unmapped, unreadable, or unwritable guest memory. Closes when: console behavior for invalid guest pointers in `CrossOriginRequest` exports is verified on hardware.
