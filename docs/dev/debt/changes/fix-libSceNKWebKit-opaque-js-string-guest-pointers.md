# libSceNKWebKit

## add Functional

- [OpaqueJSString::tryCreate guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/OpaqueJSString.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `OpaqueJSString::tryCreate(const WTF::String&)` and `OpaqueJSString::tryCreate(WTF::String&&)` is unmeasured; both exports read guest `WTF::String`, `WTF::StringImpl`, and character storage through `GuestReadable_nid_no_patch` / `GuestCopyIn_nid_no_patch`, and the rvalue overload validates writable `WTF::String` and releasable `WTF::StringImpl` storage (including cycle-bounded extended substring headers) under `GuestArena::HostWrite` before clearing `*string` via `GuestCopyOut_nid_no_patch`, calling `NotImplementedAt_nid_no_patch` on unmapped, unreadable, or unwritable guest memory. Closes when: console behavior for invalid guest pointers in `OpaqueJSString::tryCreate` exports is verified on hardware.
