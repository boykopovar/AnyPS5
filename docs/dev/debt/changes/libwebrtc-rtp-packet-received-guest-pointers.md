# libwebrtc

## add Unknown function info

- [webrtc::RtpPacketReceived lifecycle guest pointer validation](../../../core/libs/prx/libwebrtc/src/RtpPacketReceived.cpp) (libwebrtc) - `RtpPacketReceived` constructors and destructor validate guest packet, extension-map, extension-entry range, buffer and `additionalData` pointers through checked guest copies and throw `std::invalid_argument` on unreadable, misaligned or read-only guest memory without leaking allocations; console behavior for invalid pointers is unverified.
