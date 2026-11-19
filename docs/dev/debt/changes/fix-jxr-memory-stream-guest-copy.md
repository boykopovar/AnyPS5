# libSceJxr

## add Functional

- [EOSWS_Memory, GetPosWS_Memory, ReadWS_Memory, SetPosWS_Memory and WriteWS_Memory guest pointers](../../../core/libs/prx/libSceJxr/Export.cpp) (libSceJxr) - `stream` state and caller/stream byte buffers are accessed through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving caller `errno` and throwing `APS5_INVALID_ARG_EX` on unreadable or unwritable guest memory as a host-safety fallback instead of crashing on unmapped pointers. Console behaviour on unmapped memory is unmeasured.
