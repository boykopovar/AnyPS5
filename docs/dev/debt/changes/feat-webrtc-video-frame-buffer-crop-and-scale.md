# libwebrtc

## add Public source implementations

- [webrtc::VideoFrameBuffer::CropAndScale](../../../core/libs/prx/libwebrtc/src/VideoFrameBufferCropAndScale.cpp) (libwebrtc) - nothing is checked on the console. Follows [video_frame_buffer.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video/video_frame_buffer.cc) and `CropAndScaleFrom` of [i420_buffer.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video/i420_buffer.cc). The planes are scaled by `libyuv::I420Scale` from the `3rdparty/libyuv` submodule, at the revision M134's DEPS pins (`10592b60`); the module's libyuv revision is unknown. libyuv picks its row functions from the host CPU, so a host without SSSE3 can differ from the console by a rounding step. A crop rectangle outside the buffer and a null `ToI420` throw `std::invalid_argument` instead of aborting.
