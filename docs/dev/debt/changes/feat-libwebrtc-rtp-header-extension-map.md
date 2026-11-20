# libwebrtc

## add Public source implementations

- [webrtc::RtpHeaderExtensionMap constructor](../../../core/libs/prx/libwebrtc/src/RtpHeaderExtensionMap.cpp) (libwebrtc) - nothing is checked on the console. Adapted from [rtp_header_extension_map.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/modules/rtp_rtcp/source/rtp_header_extension_map.cc), whose WebRTC notice and BSD licence head the file: uses the 23-byte M134 layout (`ids[22]` + `extmap_allow_mixed`, including `kRtpExtensionCorruptionDetection` at index 21; M127 lacked it and was 22 bytes) and assumes `rtc::ArrayView` travels in two registers. A null or unwritable map, unreadable extension array or malformed/unreadable guest URI string throws `std::invalid_argument` while preserving `errno`. Closes when: compared with the console.
