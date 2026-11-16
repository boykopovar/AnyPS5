# libScePad

## add Functional

- [scePadParseUniqueDataLeonardo](../../../core/libs/prx/libScePad/Export.cpp) (libScePad) - reading Leonardo data through `GuestCopyIn_nid_no_patch` and clearing unique data through `GuestCopyOut_nid_no_patch`, returning `PAD_ERROR_INVALID_ARG` while preserving caller errno for guest pointers that cannot be accessed, is a host-safety fallback; console fault handling is unmeasured.
