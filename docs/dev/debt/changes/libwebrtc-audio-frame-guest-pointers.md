# libwebrtc

## add Unknown function info

- [webrtc::AudioFrame guest pointer validation](../../../core/libs/prx/libwebrtc/src/AudioFrame.cpp) (libwebrtc) - `AudioFrame`, `Mute`, `muted`, `data`, `mutable_data`, `UpdateFrame`, `CopyFrom` and `data_view` validate guest frame, sample and `packetInfos` accessibility through checked guest copies and throw `std::invalid_argument` on unreadable or read-only guest memory or sample-count overflow instead of faulting; console behavior for invalid pointers is unverified.
