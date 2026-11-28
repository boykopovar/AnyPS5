# libwebrtc

## add Unknown function info

- [rtc::NetworkManagerBase guest pointer validation](../../../core/libs/prx/libwebrtc/src/NetworkManager.cpp) (libwebrtc) - `enumeration_permission`, `set_default_local_addresses`, `GetDefaultLocalAddress` and `GetNetworkFromAddress` validate guest manager, address, network-list and interface-address ranges through checked guest copies and throw `std::invalid_argument` on unreadable, misaligned or read-only guest memory instead of faulting; console behavior for invalid pointers is unverified.
