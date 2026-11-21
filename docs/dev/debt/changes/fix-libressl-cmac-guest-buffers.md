# libSceLibreSsl

## add Functional

- [CMAC_Update and CMAC_Final](../../../core/libs/prx/libSceLibreSsl/src/Cmac.cpp) (libSceLibreSsl) - read the message of `CMAC_Update` and write the tag, its length and the cleared tag of `CMAC_Final` through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch`, staging each block in a wiped host buffer, so an unreadable message or an unwritable tag or length throws `std::invalid_argument` where the firmware faults on that pointer. A failed final still clears the guest tag, as before. This is a host-safety fallback for invalid or partially mapped guest pointers and was not measured on a console.
