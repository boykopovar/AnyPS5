# libwebrtc

## add Unknown function info

- [RecursiveCriticalSection.cpp](../../../core/libs/prx/libwebrtc/src/RecursiveCriticalSection.cpp): `rtc::RecursiveCriticalSection` and `rtc::CritScope` validate guest memory readability and writability with `GuestArenaPrepareHostRead`, `GuestArena::HostWrite`, `GuestCopyIn`, and `GuestCopyOut` (preflighting `CritScope` writability before acquiring the lock) and throw `std::invalid_argument` on inaccessible guest pointers because PS5 hardware fault behavior is unverified.
