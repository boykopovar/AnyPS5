# libwebrtc

## change [webrtc::AudioState::Config constructor]

- [webrtc::AudioState::Config constructor and destructor](../../../core/libs/prx/libwebrtc/src/AudioStateConfig.cpp) (libwebrtc) - nothing is checked on the console, including the `Release` vtable slot. [audio_state.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/call/audio_state.cc), whose WebRTC notice and BSD licence head the file, defaults both, so the four `scoped_refptr` members of `audio_state.h` (mixer, processing, device module and async processing factory) start null. The destructor releases them in reverse order through `Release`, slot 1 of `RefCountInterface` in [ref_count.h](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/ref_count.h), the same at M127. A null config throws. Closes when: compared with the console.
