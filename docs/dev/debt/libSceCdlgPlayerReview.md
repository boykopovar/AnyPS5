# libSceCdlgPlayerReview technical debt

### Silent stubs

- [scePlayerReviewDialogGetResult](../../../core/libs/prx/libSceCdlgPlayerReview/Export.cpp) - returning `COMMON_DIALOG_ERROR_ARG_NULL` for an unwritable result pointer is a host-safety fallback with console behavior unmeasured.

### Unknown function info

- [scePlayerReviewDialogOpen](../../../core/libs/prx/libSceCdlgPlayerReview/Export.cpp) uses a recovered 168-byte parameter layout; the 40-byte target record at byte 60 and mode semantics remain unresolved.

### Functional

- [Player review dialog](../../../core/libs/prx/libSceCdlgPlayerReview/Export.cpp) (libSceCdlgPlayerReview) shows nothing: `scePlayerReviewDialogOpen` finishes at once and the result reports that the user canceled, as `sceWebBrowserDialogOpen` does
