# libwebrtc

## add Public source implementations

- [webrtc::PeerConnectionFactoryDependencies constructors and destructor](../../../core/libs/prx/libwebrtc/src/PeerConnectionFactoryDependencies.cpp) (libwebrtc) - layout and vtable slots are unverified on the console. Adapted from [peer_connection_interface.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/peer_connection_interface.cc), whose notice and BSD licence head the file: 224 bytes (4 raw pointers, 14 `unique_ptr`s, 5 `scoped_refptr`s, 5 `unique_ptr`s through `media_factory` at 216, identical in M132–M134; M127 is 216 bytes without `audio_processing_builder`). Move keeps raw pointers on source; destructor releases refs via slot 1 and deletes `unique_ptr`s via slot 1 (slot 2 for predictor/monitor factories, slot 6 for `video_encoder_factory`) in reverse order. Invalid pointers or vtables throw while preserving `errno`. Closes when: compared with the console.
