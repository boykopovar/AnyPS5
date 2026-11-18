# libSceNKWebKit

## add Functional

- [WTF::sixCharacterHashStringToInteger](../../../core/libs/prx/libSceNKWebKit/src/WtfPrimitives.cpp) (libSceNKWebKit) reads the hash's characters and its terminator one at a time with `GuestCopyIn_nid_no_patch`, stopping at the first invalid one as before, so a string that can't be read throws `std::invalid_argument`, as a null or malformed one does. This is a host-safety fallback; the console's behaviour for an inaccessible string is not measured.
