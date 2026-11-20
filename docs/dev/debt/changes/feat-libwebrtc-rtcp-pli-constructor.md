# libwebrtc

## add Public source implementations

- [webrtc::rtcp::Pli constructor](../../../core/libs/prx/libwebrtc/src/RtcpPli.cpp) (libwebrtc) - layout, vtable and allocator are unverified on the console. The defaulted constructor of [pli.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/modules/rtp_rtcp/source/rtcp_packet/pli.cc) zeroes `sender_ssrc_` at 8 and `media_ssrc_` at 12 and installs a local vtable of D1, D0 (D1 then guest scalar delete), `BlockLength` (12) and `Create`. `Create` writes the version 2, FMT 1, PT 206 header with length 2 and both SSRCs big-endian, as `Pli::Create`, `RtcpPacket::CreateHeader` and `Psfb::CreateCommonFeedback` do at M134; a full buffer is handed to the callback first, and a full buffer without one throws instead of calling a null `FunctionView`. A null packet throws. Closes when: compared with the console.
