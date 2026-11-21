# libwebrtc

## add Public source implementations

- [webrtc::AudioFrame::muted, data, mutable_data, UpdateFrame and data_view](../../../core/libs/prx/libwebrtc/src/AudioFrame.cpp) (libwebrtc) - the frame fields these read and write, and the samples `UpdateFrame` copies in, go through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch`, so a frame or sample buffer that can't be read or written throws `std::invalid_argument`, as a null frame does. This is a host-safety fallback: upstream dereferences the frame, and the console's behaviour for an inaccessible one is not measured. `UpdateFrame` still stores the fields one by one before the sample-count check, so a frame with unreadable samples keeps the new fields and its old samples. Closes when: compared with the console.
