# libwebrtc

## add Public source implementations

- [webrtc::AudioSendStream::Config and AudioReceiveStreamInterface::Config constructors](../../../core/libs/prx/libwebrtc/src/AudioStreamConfig.cpp) (libwebrtc) - nothing is checked on the console. Follows [audio_send_stream.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/call/audio_send_stream.cc), [audio_receive_stream.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/call/audio_receive_stream.cc) and [crypto_options.h](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/crypto/crypto_options.h), whose WebRTC notice and BSD licence head the file: `CryptoOptions` uses the M134 default `enable_encrypted_rtp_header_extensions = true` (M127 defaulted to `false`). A null or unwritable config throws `std::invalid_argument` via checked guest copy while preserving `errno`. Closes when: compared with the console.
