# libSceNKWebKit

## add Functional

- [JscValue exception guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/JscValue.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest `exception` output pointers in `JSValueCreateJSONString`, `JSValueIsEqual`, `JSValueIsInstanceOfConstructor`, `JSValueToNumber`, `JSValueToStringCopy`, and `JSValueToObject` is unmeasured; the exports validate writable guest storage under `GuestArena::HostWrite` and write exception handles through `GuestCopyOut_nid_no_patch`, calling `NotImplementedAt_nid_no_patch` on unmapped or unwritable `exception` pointers. Closes when: console behavior for invalid guest `exception` pointers in `JSValue*` exports is verified on hardware.
