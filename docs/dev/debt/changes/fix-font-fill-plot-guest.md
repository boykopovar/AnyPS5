# libSceFont

## add Unknown function info

- [Fill plot guest input and output](../../../core/libs/prx/libSceFont/src/Graphics.cpp): `sceFontGraphicsFillPlotInit` reads a real region through checked guest copy, builds the result locally and copies it out through checked guest memory. Invalid regions clear only the tag, preserving other bytes. Unreadable inputs return `FONT_GRAPHICS_ERROR_INVALID_REGION`; unwritable outputs return `FONT_GRAPHICS_ERROR_INVALID_FILL_PLOT`. These inaccessible-guest-memory outcomes are host-safety fallback policies, not measured PS5 ABI behavior.
