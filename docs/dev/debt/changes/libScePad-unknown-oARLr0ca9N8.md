# libScePad

## remove `oARLr0ca9N8`

## add Unknown function info

- [libScePadUnknown_oARLr0ca9N8_nid_no_patch](../../../core/libs/prx/libScePad/InventoryExports.cpp) (libScePad, NID `oARLr0ca9N8`) - the symbol name is unverified and comes from the 13.60 export inventory; behaviour follows `libScePad.sprx` disassembly (not measured on a retail console), returning `PAD_ERROR_NOT_INITIALIZED` before `scePadInit`, `PAD_ERROR_INVALID_ARG` for a null `out`, and `PAD_ERROR_INVALID_HANDLE` for an unknown handle before writing `0`. Returning `PAD_ERROR_INVALID_ARG` with caller `errno` preserved for an unwritable non-null `out` pointer via `GuestCopyOut_nid_no_patch` is a host-safety fallback with console fault handling unmeasured.
