# libwebrtc

## add Public source implementations

- [webrtc::SdpVideoFormat::IsCodecInList](../../../core/libs/prx/libwebrtc/src/SdpVideoFormatCodecList.cpp) (libwebrtc) - not checked on the console. Adapted from [sdp_video_format.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video_codecs/sdp_video_format.cc) and its H264, VP9, AV1 and string-to-number helpers, whose logic is the same at M127. The `ArrayView` comes as pointer and count. Names match ASCII case-insensitively; H264, VP9 and AV1 also compare profiles and default parameters. A matching H265 name throws: its rule depends on `RTC_ENABLE_H265` and changed between M127 and M134. Invalid formats or maps with duplicate keys throw. Closes when: compared with the console.
