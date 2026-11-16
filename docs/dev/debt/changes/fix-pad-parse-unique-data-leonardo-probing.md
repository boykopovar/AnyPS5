# libScePad

## add Functional

- [scePadParseUniqueDataLeonardo](../../../core/libs/prx/libScePad/Export.cpp) (libScePad) - clearing 0x18 bytes of unique data through `GuestCopyOut_nid_no_patch` independently of input readability and probing only the first byte of the unverified Leonardo data pointer through `GuestCopyIn_nid_no_patch` until record size is known, returning `PAD_ERROR_INVALID_ARG` while preserving caller errno when guest pointers cannot be accessed, is a host-safety fallback; console fault handling is unmeasured.
