# libwebrtc

## add Public source implementations

- [rtc::IPAddress::ipv4_address, ipv6_address, v4AddressAsHostOrderInteger and operator==, rtc::CountIPMaskBits and rtc::TruncateIP](../../../core/libs/prx/libwebrtc/src/IpAddress.cpp) (libwebrtc) - each address is copied in with one `GuestCopyIn_nid_no_patch`, and `TruncateIP` builds its result locally and writes it with one `GuestCopyOut_nid_no_patch`, so an address that can't be read, or a result that can't be written, throws `std::invalid_argument`, as a null one does. This is a host-safety fallback: upstream dereferences these pointers, and the console's behaviour for inaccessible ones is not measured. Closes when: compared with the console.
