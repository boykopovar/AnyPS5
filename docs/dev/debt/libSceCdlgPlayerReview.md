# libSceCdlgPlayerReview technical debt

### Unknown function info

- [scePlayerReviewDialogOpen](../../../core/libs/prx/libSceCdlgPlayerReview/Export.cpp) uses a recovered 168-byte parameter layout; the 40-byte target record at byte 60 and mode semantics remain unresolved.

### Functional

- [Player review dialog](../../../core/libs/prx/libSceCdlgPlayerReview/Export.cpp) (libSceCdlgPlayerReview) shows nothing: `scePlayerReviewDialogOpen` finishes at once and the result reports that the user canceled, as `sceWebBrowserDialogOpen` does; returning `COMMON_DIALOG_ERROR_INVALID_ARGUMENT` for an unwritable result is a host-safety fallback as console behavior for unwritable memory has not been measured
