# libwebrtc

## add Public source implementations

- [webrtc::SmoothingFilterImpl constructor](../../../core/libs/prx/libwebrtc/src/SmoothingFilterImpl.cpp) (libwebrtc) - the 64-byte layout, the vtable order (D1, D0, `AddSample`, `GetAverage`, `SetTimeConstantMs`) and the optional end time's flag after its value are not checked on a console. The vtable is local and has no type information, so `typeid` and `dynamic_cast` on the object are not supported. Whether the console build contracts the state update into fused multiply-adds is unverified; here `powf`, `expf` and `exp` come from the guest libc. Adapted from [smoothing_filter.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/common_audio/smoothing_filter.cc), the same at M127 apart from `absl::optional`. Closes when: measured on a console.
