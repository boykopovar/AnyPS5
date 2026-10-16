# libSceFontFt technical debt

### Silent stubs

- [libSceFontFt](../../../core/libs/prx/libSceFontFt/Export.cpp) returns success for alias lifecycle, format support queries and glyph selection without maintaining aliases or exposing FreeType capability details. The no-argument signatures and successful compatibility behavior follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/font/fontft.cpp), while actual alias registration and per-format support remain unavailable
