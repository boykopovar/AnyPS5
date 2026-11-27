# libwebrtc

## add Public source implementations

- [webrtc::RtpPacketReceived constructors and destructor](../../../core/libs/prx/libwebrtc/src/RtpPacketReceived.cpp) (libwebrtc) - layout and allocator are unverified on the console. The 144-byte layout follows `RtpPacket` and `RtpPacketReceived` of [rtp_packet_received.h](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/modules/rtp_rtcp/source/rtp_packet_received.h) at M134. The `(const ExtensionManager*, Timestamp)` constructor copies the map (or an empty one), reserves the default 1500-byte buffer and writes the 12-byte header with version 2, as `RtpPacket::Clear` does. The copy constructor copies the extension entries into a guest allocation sized to fit, shares the buffer and adds a reference to `additional_data_`; the destructor drops them and calls the deleting destructor (vtable slot 1) when the last reference goes. A null object throws. Closes when: compared with the console.
