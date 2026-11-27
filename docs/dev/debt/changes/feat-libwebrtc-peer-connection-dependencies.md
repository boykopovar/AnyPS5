# libwebrtc

## add Public source implementations

- [webrtc::PeerConnectionDependencies constructors and destructor](../../../core/libs/prx/libwebrtc/src/PeerConnectionDependencies.cpp) (libwebrtc) - layout and vtable dispatch are unverified on the console. Adapted from [peer_connection_interface.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/peer_connection_interface.cc), whose WebRTC notice and BSD licence head the file: 72 bytes (`observer` at 0 and 8 `unique_ptr` factories/interfaces through `trials` at 64, identical across M132 to M134; M127 is 64 bytes without `network_controller_factory`). Move keeps `observer` on the source; destructor deletes non-null owned objects via vtable slot 1 in reverse member order. Null, unwritable, unmapped, overlapping or malformed objects throw `std::invalid_argument` while preserving `errno`. Closes when: compared with the console.
