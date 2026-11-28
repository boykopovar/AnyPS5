# libwebrtc

## add Public source implementations

- [rtc::TruncateIP](../../../core/libs/prx/libwebrtc/src/IpAddress.cpp) (libwebrtc) - the result's vtable word is left at 0 instead of the `IPAddress` vtable, which is not modelled; only `family_` and `u_` are written, as the `IPAddress` copy constructor does. The result is returned through the hidden pointer the Itanium C++ ABI uses for a class with a virtual destructor, not checked on the console. Adapted from [ip_address.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/rtc_base/ip_address.cc), with the same logic at M127 and M134, with its WebRTC notice and BSD licence at the top of the file. Closes when: compared with the console.
