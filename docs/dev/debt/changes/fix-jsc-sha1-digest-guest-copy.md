# libSceJsc

## add Functional

- [SHA1 digest output](../../../core/libs/prx/libSceJsc/src/WtfSha1.cpp) (libSceJsc) - `WTF::SHA1::computeHash` writes the 20-byte digest with one `GuestCopyOut_nid_no_patch`. A digest that cannot be written throws `std::invalid_argument` after the hasher is reset. This is a host-safety fallback; whether the console faults or returns an error for an unmapped digest is unmeasured.
