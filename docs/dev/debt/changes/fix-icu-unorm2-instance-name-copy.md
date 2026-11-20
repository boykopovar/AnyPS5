# libSceIcu

## add Unknown function info

- [unorm2_getInstance name](../../../core/libs/prx/libSceIcu/Normalization.cpp) (libSceIcu) - when the status is not a failure, the guest name is copied in up to its terminator with `GuestCopyIn_nid_no_patch` in page-bounded chunks and ICU receives the host copy; an unreadable name throws `std::invalid_argument` with `errno` preserved, because the console's behaviour on unmapped memory is unmeasured. A failing status returns before the name is read. The package is only compared with null, and the `UErrorCode` is still read and written in place, as everywhere in this library.
