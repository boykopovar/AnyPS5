# libwebrtc

## add Public source implementations

- [webrtc::RtpPacket::Parse(const uint8_t*, size_t)](../../../core/libs/prx/libwebrtc/src/RtpPacket.cpp) (libwebrtc) - console ABI and behaviour are unverified. Adapted from [rtp_packet.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/modules/rtp_rtcp/source/rtp_packet.cc), whose parsing is the same at M127, with copy_on_write_buffer.h and buffer.h, retaining their notices and BSD licence. The 112-byte layout, the same from M127 to M134, assumes the module's libc++, as `CodecSpecificInfo` does: extension entries at 56, buffer at 88. The entry vector grows like libc++ through guest scalar new/delete. Release logs and DCHECKs are omitted. Throws for a null packet, a null buffer of 12 or more bytes, and where release upstream would write through a missing or under-12-byte buffer while clearing after a failed parse. Closes when: compared with the console.
