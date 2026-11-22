# libwebrtc

## add Public source implementations

- [webrtc::AudioFrame::CopyFrom](../../../core/libs/prx/libwebrtc/src/AudioFrame.cpp) (libwebrtc) - follows [audio_frame.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/audio/audio_frame.cc) on the existing 15456-byte frame layout: a self copy does nothing, a muted target is zeroed before an unmuted source is copied, every field but `profile_timestamp_ms_` is copied, and only an unmuted source's `num_channels * samples_per_channel` samples are copied. The `RtpPacketInfos` reference is shared: the source's data gains a reference and the target's old data loses one, freeing it as the `EncodedImage` destructor does. Upstream's `RTC_CHECK` on the sample count throws instead of aborting, and null frames throw. Not compared with the console.
