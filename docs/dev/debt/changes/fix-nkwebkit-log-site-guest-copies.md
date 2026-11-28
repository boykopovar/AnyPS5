# libSceNKWebKit

## add Functional

- [WTF::Logger::LogSiteIdentifier::toString](../../../core/libs/prx/libSceNKWebKit/src/WtfLogger.cpp) (libSceNKWebKit) copies the 24-byte identifier in with `GuestCopyIn_nid_no_patch` and its class and method names with `CopyGuestString_nid_no_patch`, at most 65535 bytes each. An identifier or name that can't be read, or a name with no terminator within that limit, throws `std::invalid_argument`, as a null identifier or method name does. This is a host-safety fallback; the console's behaviour for inaccessible pointers is not measured.
