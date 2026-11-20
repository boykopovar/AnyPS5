# libwebrtc

## add Public source implementations

- [webrtc::RtpTransceiverInit constructor and destructor](../../../core/libs/prx/libwebrtc/src/RtpTransceiverInit.cpp) (libwebrtc) - nothing is checked on the console. The assumed 56-byte outer layout holds vectors of 40-byte strings and 320-byte `RtpEncodingParameters`; the optional 120-byte `RtpCodec` owns a feedback vector and string map. M127 and M134 have the same destruction order; their renamed resolution member remains `optional<Resolution>`. Null objects and malformed ownership throw before release. Adapted from [rtp_transceiver_interface.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/rtp_transceiver_interface.cc) and [rtp_parameters.h](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/rtp_parameters.h), whose notices and BSD licence head the file. Closes when: compared with the console.
