# libSceNKWebKit

## add Functional

- [WTF::Internal::parseDoubleFromLongString](../../../core/libs/prx/libSceNKWebKit/src/WtfString.cpp) (libSceNKWebKit) copies the caller's UTF-16 units in with `GuestCopyIn_nid_no_patch`, 2048 at a time, and writes the parsed length with `GuestCopyOut_nid_no_patch`. A string that can't be read or a parsed-length output that can't be written throws `std::invalid_argument`. This is a host-safety fallback; the console's behaviour for inaccessible pointers is not measured.
