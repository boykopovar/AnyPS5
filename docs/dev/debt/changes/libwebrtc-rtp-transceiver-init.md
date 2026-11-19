# libwebrtc

## add Public source implementations

- [webrtc::RtpTransceiverInit default constructor](../../../core/libs/prx/libwebrtc/src/RtpTransceiverInit.cpp) (libwebrtc) - nothing is checked on the console; the layout follows the pinned header, not the console's `libwebrtc.sprx`: `direction` at 0 set to `kSendRecv` (0), then `stream_ids` at 8 and `send_encodings` at 32 as empty three-pointer vectors, 56 bytes in all. A null object throws. The destructor is still a stub, because destroying `send_encodings` needs the unmodelled `RtpEncodingParameters` layout. Adapted from [rtp_transceiver_interface.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/rtp_transceiver_interface.cc) and [rtp_transceiver_interface.h](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/rtp_transceiver_interface.h), whose WebRTC notice and BSD licence head the file. Closes when: compared with the console.
