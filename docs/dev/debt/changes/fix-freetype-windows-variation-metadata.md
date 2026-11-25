# libSceFreeType

## change [FT_Get_MM_Var]

- [FT_Get_MM_Var](../../../core/libs/prx/libSceFreeType/src/Variations.cpp) - console signature and behavior are unverified. The adapter uses bundled [FreeType 2.15.0](https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/base/ftmm.c). On Windows it widens axis bounds, tags and named-instance coordinates into one LP64 block, with axis flags directly after the header and names copied into the block. Returned values have the native backend's range. An unwritable output returns `FT_Err_Invalid_Argument` and releases any allocated records. Measured against native FreeType with synthetic OpenType and Type 1 multiple-master fonts, not against a console.

## change [FT_Done_MM_Var]

- [FT_Done_MM_Var](../../../core/libs/prx/libSceFreeType/src/Variations.cpp) - console behavior is unverified. It releases the LP64 block returned by the paired guest getter on Windows and uses the native FreeType deallocator when the layouts already match. The caller must pass the originating library and a record returned by that getter. An unreadable library pointer throws as a host-safety fallback.

## change [FT_Get_Multi_Master]

- [FT_Get_Multi_Master](../../../core/libs/prx/libSceFreeType/src/Variations.cpp) - console signature and behavior are unverified. It follows bundled [FreeType 2.15.0](https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/type1/t1load.c#L178-L209): success writes the counts and reported Type 1 axes only; an error leaves the output unchanged. On Windows the signed bounds are widened to 64 bits. The axis names are borrowed from the face. Tested with a synthetic Type 1 multiple-master font and OpenType rejection on Windows.
