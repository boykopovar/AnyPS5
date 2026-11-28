# libSceNKWebKit

## add Functional

- [CrossOriginPreflightResultCache guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/CrossOriginPreflightResultCache.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `CrossOriginPreflightResultCache::appendEntry` and `canSkipPreflight` is unmeasured; both exports validate guest readability and copy nested `WTF::String`, `WTF::URL`, `HTTPHeaderMap`, and guest cache-item hash tables via `GuestCopyIn_nid_no_patch`, and `appendEntry` validates writable `unique_ptr` and releasable `WTF::StringImpl` entries under active `GuestArena::HostWrite` guards before clearing `*preflightResult` via `GuestCopyOut_nid_no_patch`, calling `NotImplementedAt_nid_no_patch` on invalid guest memory. Closes when: console behavior for invalid guest pointers in `CrossOriginPreflightResultCache` exports is verified on hardware.
