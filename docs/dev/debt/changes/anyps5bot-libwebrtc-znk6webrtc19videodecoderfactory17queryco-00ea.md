# libwebrtc

## add Public source implementations

- [webrtc::VideoDecoderFactory::QueryCodecSupport and webrtc::SdpVideoFormat::IsCodecInList](../../../core/libs/prx/libwebrtc/src/VideoDecoderFactory.cpp) (libwebrtc) - comparing two H265 formats throws through `NotImplemented_nid_no_patch`: whether `RTC_ENABLE_H265` is defined is unknown and the H265 rule differs between M127 and M134. Assumed, not measured: the 112-byte `SdpVideoFormat` layout, `GetSupportedFormats` at vtable slot 2, `rtc::ArrayView` passed as data and size, and `CodecSupport` returned as two bytes. Malformed objects throw `std::invalid_argument`. Adapted from [WebRTC api/video_codecs](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video_codecs/). Closes when: compared with the console.
