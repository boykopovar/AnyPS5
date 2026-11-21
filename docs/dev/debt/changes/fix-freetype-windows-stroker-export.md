# libSceFreeType

## change [FT_Stroker_ExportBorder, FT_Stroker_Export]

- [FT_Stroker_ExportBorder, FT_Stroker_Export](../../../core/libs/prx/libSceFreeType/src/Stroker.cpp) (libSceFreeType) - console signatures, FreeType version and behavior are unverified. On Windows the wrapper remembers each successful border count without counting during export, appends the selected, counted borders in FreeType order, widens native 32-bit coordinates to guest 64-bit coordinates, and offsets contour ends by the original point count. Modifying a counted stroke requires counting again; stale counts, inaccessible output storage and sums above the 16-bit count limit throw before writing. The caller still supplies the arrays with room for the append. On LP64 hosts the wrapper forwards to [FreeType 2.15.0](https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/base/ftstroke.c#L1938-L2041), including native count wrap. Windows tests compare both exports and the `libSceFreeTypeOt` forwarder with native FreeType, not a console.

# libSceFreeTypeOt

## change [FT_Stroker_Export]

- [FT_Stroker_Export](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) (libSceFreeTypeOt, NID `2rcvKZfm5rI`) - the console prototype is unverified and this provider reports FreeType 2.4.11. It uses [libSceFreeType's stroker export](libSceFreeType.md), including checked LP64 output conversion on Windows. [2.4.11](https://github.com/freetype/freetype/blob/c6a66b49e64e9a21c013f23c867d238cf1aa98cb/src/base/ftstroke.c#L2008-L2033) has no null checks and adds to signed 16-bit counts; it was not used as the test reference.

## change [FT_Stroker_ExportBorder and FT_Stroker_ParseOutline]

- [FT_Stroker_ExportBorder and FT_Stroker_ParseOutline](../../../core/libs/prx/libSceFreeTypeOt/ForwardExports.cpp) (`7cIS2cFG3PE`, `RvFx7dv9ThU`) use the existing [libSceFreeType wrappers](libSceFreeType.md). The public prototypes are unchanged from FreeType 2.4.11 to 2.15.0, but the provider's console ABI and behavior are unverified. Windows output follows the host's FreeType 2.15.0 after coordinate widening, not the older provider's internal arithmetic.
