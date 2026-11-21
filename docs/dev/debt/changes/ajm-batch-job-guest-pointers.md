# libSceAjm.native

## add Unknown function info

- [Batch job builders](../../../core/libs/prx/libSceAjm.native/src/Ajm.cpp) (libSceAjm.native) - copy `AjmBatchInfo`, `codec_parameters`, `gapless_decode`, split buffer lists and packed job outputs via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` (preserving `errno`), returning `0x80930005` (`SCE_AJM_ERROR_INVALID_PARAMETER`) when guest memory is unreadable or unwritable; PS5 console behavior on unmapped pointers in these builders is unverified.
