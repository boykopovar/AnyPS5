# libSceFont

## add Functional

- [Style frame records](../../../core/libs/prx/libSceFont/src/Style.cpp) (libSceFont) - every `sceFontStyleFrame*` call reads the caller's frame through a checked copy, `sceFontStyleFrameInit` writes the whole frame in one copy, and the other setters write only the fields they change. An unreadable or unwritable frame returns `SCE_FONT_ERROR_INVALID_PARAMETER` with `errno` preserved; the console's result for an unmapped frame is not measured.
