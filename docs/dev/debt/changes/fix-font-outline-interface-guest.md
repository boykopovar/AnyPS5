# libSceFont

## add Unknown function info

- [Outline vertex interface guest result](../../../core/libs/prx/libSceFont/src/Graphics.cpp): `sceFontOutlineVertexesInterfaceInit` checks the entire 48-byte guest output copy, preserves `errno` and reports `SCE_FONT_ERROR_INVALID_PARAMETER` for an inaccessible pointer rather than crashing the host. The error is an unmeasured host-safety policy, not console ABI evidence.
