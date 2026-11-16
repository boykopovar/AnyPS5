# libSceFont

## add Functional

- [Font memory records and library handles](../../../core/libs/prx/libSceFont/src/Library.cpp) (libSceFont) - `sceFontMemoryInit` and `sceFontMemoryTerm` read and write the caller's `FontMemory` through checked copies, and `sceFontDestroyLibrary` and `sceFontDestroyRenderer` read and clear the caller's handle the same way. An unreadable or unwritable record or handle returns `SCE_FONT_ERROR_INVALID_PARAMETER` before anything is released. The memory interface table that `sceFontMemoryTerm` calls through is still read in place. A host-safety fallback, not measured on the console.
