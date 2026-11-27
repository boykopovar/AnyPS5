# libSceNKWebKit

## add Functional

- [FloatSize(IntSize) guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/FloatSize.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::FloatSize::FloatSize(const WebCore::IntSize&)` is unmeasured; both constructor entry points validate writable guest storage with `GuestArena::HostWrite` and `GuestWritable_nid_no_patch`, read `WebCore::IntSize` via `GuestCopyIn_nid_no_patch`, and publish `WebCore::FloatSize` via `GuestCopyOut_nid_no_patch`, throwing `std::invalid_argument` on unwritable `self` and calling `NotImplementedAt_nid_no_patch` with the entry point's own symbol name on unreadable `size`. Closes when: console behavior for invalid guest pointers in `WebCore::FloatSize::FloatSize(const WebCore::IntSize&)` is verified on hardware.
