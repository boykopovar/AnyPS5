# libwebrtc

## add Public source implementations

- [rtc::SocketAddress copy constructor and assignment](../../../core/libs/prx/libwebrtc/src/SocketAddressCopy.cpp) (libwebrtc) - the copy constructor leaves the `ip_` vtable word at 0 instead of the `IPAddress` vtable, which is not modelled; both copy only `family_` and `u_` of the address, as `IPAddress`'s copy operations do. The hostname follows the guest `std::string` assignment rules of `BasicString.cpp` (reuse the buffer when it fits, otherwise allocate `size | 15`); the layout is not measured on the console. Adapted from [socket_address.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/rtc_base/socket_address.cc), the same at M127 and M134. Closes when: compared with the console.
