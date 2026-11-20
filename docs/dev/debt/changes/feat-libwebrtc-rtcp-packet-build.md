# libwebrtc

## add Public source implementations

- [webrtc::rtcp::RtcpPacket::Build()](../../../core/libs/prx/libwebrtc/src/RtcpPacket.cpp) (libwebrtc) - the vtable slots and allocator are unverified on the console. Adapted from [rtcp_packet.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/modules/rtp_rtcp/source/rtcp_packet.cc), retaining its notice: the returned 24-byte `rtc::Buffer` (size, capacity, data) gets `BlockLength()` bytes from guest `new[]`, as the `BufferT(size_t)` constructor in `buffer.h` does, and `Create` (vtable slot 3, after D1, D0 and `BlockLength`) fills it from index 0 with a null callback. Create's result and the final length are only `RTC_DCHECK`ed upstream and are not checked. The buffer is returned through the hidden result pointer. Null results or packets, and packets without both virtual functions, throw. Closes when: compared with the console.
