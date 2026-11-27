# libSceFreeTypeOl

## add Functional

- [libSceFreeTypeOl charmap guest pointer fault behavior](../../../core/libs/prx/libSceFreeTypeOl/Charmap.cpp) (libSceFreeTypeOl) - Console fault behavior for invalid guest pointers in `sceFreeTypeOlCMapNew` and `sceFreeTypeOlCMapDone` is unmeasured; exports validate required guest reads (`GuestArenaPrepareHostRead_nid_no_patch` + `GuestReadable_nid_no_patch`) before `GuestCopyIn_nid_no_patch`, guard guest output writes with active `GuestArena::HostWrite` scopes and `GuestWritable_nid_no_patch` before `GuestCopyOut_nid_no_patch`, and report failed guest-memory accesses through `NotImplemented_nid_no_patch`. Closes when: console behavior for invalid guest pointers in `sceFreeTypeOlCMapNew` and `sceFreeTypeOlCMapDone` is verified on hardware.
