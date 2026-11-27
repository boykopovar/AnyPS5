# libSceNKWebKit

## add Functional

- [AuthenticationChallengeBase guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/AuthenticationChallengeBase.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::AuthenticationChallengeBase::isNull`, `WebCore::AuthenticationChallengeBase::nullify`, and `WebCore::AuthenticationChallenge::platformCompare` is unmeasured; the exports validate writable storage under `GuestArena::HostWrite` and copy guest `AuthenticationChallenge`, `WTF::StringImpl`, and character data through `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch`, throwing `std::invalid_argument` on unwritable `nullify` storage and calling `NotImplementedAt_nid_no_patch` on unmapped or unreadable guest memory. Closes when: console behavior for invalid guest pointers in `AuthenticationChallengeBase` exports is verified on hardware.
