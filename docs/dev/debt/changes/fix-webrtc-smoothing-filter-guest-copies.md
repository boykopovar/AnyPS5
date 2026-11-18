# libwebrtc

## add Public source implementations

- [webrtc::SmoothingFilterImpl constructor, AddSample, GetAverage and SetTimeConstantMs](../../../core/libs/prx/libwebrtc/src/SmoothingFilterImpl.cpp) (libwebrtc) - each copies the 64-byte filter in with one `GuestCopyIn_nid_no_patch`, works on the copy and writes it back with one `GuestCopyOut_nid_no_patch`, so a filter that can't be read or written throws `std::invalid_argument`. This is a host-safety fallback: upstream dereferences the filter, and the console's behaviour for an inaccessible one is not measured. Closes when: compared with the console.
