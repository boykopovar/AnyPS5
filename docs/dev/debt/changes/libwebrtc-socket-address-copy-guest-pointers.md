# libwebrtc

## add Unknown function info

- [rtc::SocketAddress copy constructor and assignment guest pointer validation](../../../core/libs/prx/libwebrtc/src/SocketAddressCopy.cpp) (libwebrtc) - `SocketAddress(const SocketAddress&)` and `operator=` validate guest source, destination and heap hostname storage through checked guest copies and throw `std::invalid_argument` on unreadable, read-only or malformed guest memory instead of faulting; console behavior for invalid pointers is unverified.
