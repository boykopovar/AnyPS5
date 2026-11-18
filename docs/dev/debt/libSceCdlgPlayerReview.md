# libSceCdlgPlayerReview technical debt

### Functional

- [scePlayerReviewDialogOpen](../../../core/libs/prx/libSceCdlgPlayerReview/Export.cpp) - returning `0x80B8000D` (`COMMON_DIALOG_ERROR_ARG_NULL`) for an unmapped or unreadable parameter pointer is a host-safety fallback; whether console hardware faults or returns an argument error on unmapped memory is unmeasured.
- [scePlayerReviewDialogGetResult](../../../core/libs/prx/libSceCdlgPlayerReview/Export.cpp) - returning `COMMON_DIALOG_ERROR_INVALID_ARGUMENT` for an unwritable result pointer is a host-safety fallback with console behavior unmeasured.

### Silent stubs

- [Player review dialog](../../../core/libs/prx/libSceCdlgPlayerReview/Export.cpp) (libSceCdlgPlayerReview) shows nothing: `scePlayerReviewDialogOpen` finishes at once and the result reports that the user canceled, as `sceWebBrowserDialogOpen` does.

### Unknown function info

- [scePlayerReviewDialogOpen](../../../core/libs/prx/libSceCdlgPlayerReview/Export.cpp) uses a recovered 168-byte parameter layout; the 40-byte target record at byte 60 and mode semantics remain unresolved.
