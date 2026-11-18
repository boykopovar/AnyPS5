# libSceFont

## add Functional

- [sceFontCreateWritingLine guest memory](../../../core/libs/prx/libSceFont/src/WritingLine.cpp) (libSceFont) - returning `SCE_FONT_ERROR_INVALID_MEMORY` with caller `errno` preserved when `memory` or `memory->iface` is unreadable via `GuestCopyIn_nid_no_patch` is a host-safety fallback; console behavior on unmapped memory is unmeasured.
