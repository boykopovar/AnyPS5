# libSceLibcInternal

## add Functional

- [_Gettime](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) copies its time string in with `CopyGuestString_nid_no_patch` (up to 64 KiB, as `_Gentime` does) and writes the field length with one `GuestCopyOut_nid_no_patch`, so an unreadable string or an unwritable length throws `std::invalid_argument`. This is a host-safety fallback; the console's behaviour for an inaccessible string or length is not measured.
