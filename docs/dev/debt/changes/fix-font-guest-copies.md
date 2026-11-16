# libSceFont

## add Functional

- [Style frame records](../../../core/libs/prx/libSceFont/src/Style.cpp) (libSceFont) - every `sceFontStyleFrame*` call reads the caller's frame through a checked copy, `sceFontStyleFrameInit` writes the whole frame in one copy, and the other setters write only the fields they change. An unreadable or unwritable frame returns `SCE_FONT_ERROR_INVALID_PARAMETER` with `errno` preserved; the console's result for an unmapped frame is not measured.
- [Font memory records and library handles](../../../core/libs/prx/libSceFont/src/Library.cpp) (libSceFont) - `sceFontMemoryInit` and `sceFontMemoryTerm` read and write the caller's `FontMemory` through checked copies, and `sceFontDestroyLibrary` and `sceFontDestroyRenderer` read and clear the caller's handle the same way. An unreadable or unwritable record or handle returns `SCE_FONT_ERROR_INVALID_PARAMETER` before anything is released. The memory interface table that `sceFontMemoryTerm` calls through is still read in place. A host-safety fallback, not measured on the console.
