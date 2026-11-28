# libwebrtc

## add Public source implementations

- [webrtc::AudioBuffer::CopyFrom and CopyTo](../../../core/libs/prx/libwebrtc/src/AudioBuffer.cpp) (libwebrtc) - the stream config is read with one `GuestCopyIn_nid_no_patch`. `CopyFrom` copies the `num_frames * num_channels` input samples in once before it changes the buffer, and `CopyTo` interleaves into a host array and writes it with one `GuestCopyOut_nid_no_patch`, so a config or sample buffer that can't be read or written throws `std::invalid_argument`, as a null one does, and the guest output is left untouched. This is a host-safety fallback: upstream dereferences them, and the console's behaviour for an inaccessible pointer is not measured. Closes when: compared with the console.
