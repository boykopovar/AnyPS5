# libSceFont

## add Functional

- [sceFontWritingLineWritesOrder guest input](../../../core/libs/prx/libSceFont/src/WritingLine.cpp) (libSceFont) - safely copy in writingMetrics via GuestCopyIn_nid_no_patch preserving host errno, returning SCE_FONT_ERROR_INVALID_PARAMETER on unmapped or invalid guest memory as a host-safety fallback; whether console hardware faults or returns an argument error on unmapped memory is unmeasured.
