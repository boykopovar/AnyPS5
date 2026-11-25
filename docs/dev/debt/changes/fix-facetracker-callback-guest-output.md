# libSceFaceTracker

## add Implemented

- [The five callback registrations](../../../core/libs/prx/libSceFaceTracker/Export.cpp) retain the verified callback and user-argument slot offsets and use `GuestCopyOut_nid_no_patch` for both writes, preserving caller `errno`. An unwritable slot returns `0x812a0001` as a host-safety fallback rather than faulting the host. The writes remain ordered; failure of the second write does not roll back the first. Console behavior for inaccessible callback slots is unmeasured.
