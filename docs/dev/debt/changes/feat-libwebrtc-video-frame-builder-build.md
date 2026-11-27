# libwebrtc

## add Public source implementations

- [webrtc::VideoFrame::Builder::build](../../../core/libs/prx/libwebrtc/src/VideoFrameBuilder.cpp) (libwebrtc) - unchecked on the console. Adapted from [video_frame.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video/video_frame.cc), whose WebRTC notice and BSD licence head the file: the frame is written through the hidden result pointer with the builder's fields in the `VideoFrame` order, `processing_time_` unset. It adds a reference on the frame buffer through its virtual `AddRef` and on the packet infos, taking `RtpPacketInfos::Data`, a `RefCountedNonVirtual`, to hold its count at offset 0. A builder without a frame buffer throws, where `RTC_CHECK` aborts; a null builder or result, or a buffer without a vtable or `AddRef`, throws. The 192-byte `Builder` and 216-byte `VideoFrame` layouts are assumed. Closes when: compared with the console.
