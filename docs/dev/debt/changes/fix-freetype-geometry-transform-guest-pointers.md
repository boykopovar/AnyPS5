# libSceFreeType

## change [FT_Outline_Embolden]

- [FT_Outline_Embolden](../../../core/libs/prx/libSceFreeType/src/OutlineTransform.cpp) (libSceFreeType) - console signature is unverified; the adapter follows bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftoutln.c) on a native copy of the guest outline, with the points published back to the guest outline. Strengths -1, 0 and 1 are native no-ops and return before point conversion; null outlines, and outlines whose record or point, tag or contour arrays are unreadable or whose points are unwritable, return the native null-outline error without writing; the console's result for unreadable outlines is unmeasured. Other guest values outside the host `FT_Pos` range throw on Windows, where the native type is 32 bits

## change [FT_Outline_EmboldenXY]

- [FT_Outline_EmboldenXY](../../../core/libs/prx/libSceFreeType/src/OutlineTransform.cpp) (libSceFreeType) - console signature is unverified; the adapter follows bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftoutln.c) on a native copy of the guest outline, with the points published back to the guest outline. When both strengths truncate to zero after division by two, the native no-op returns before point conversion; null outlines, and outlines whose record or point, tag or contour arrays are unreadable or whose points are unwritable, return the native null-outline error without writing; the console's result for unreadable outlines is unmeasured. Other guest values outside the host `FT_Pos` range throw on Windows, where the native type is 32 bits

## change [FT_Outline_Transform]

- [FT_Outline_Transform](../../../core/libs/prx/libSceFreeType/src/OutlineTransform.cpp) (libSceFreeType) - console signature is unverified; the adapter follows bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftoutln.c) on a native copy of the guest outline, with the transformed points published back to the guest outline. Hosts with 32-bit `FT_Pos` use the guest vector adapter's LP64 sums instead of narrowing the transformed coordinates. A guest input outside the host `FT_Pos` range still throws on Windows; transformed output can exceed that range. Hosts with 64-bit `FT_Pos` retain native FreeType arithmetic. An unreadable outline record, matrix or outline array, or unwritable points, leave the outline unchanged; the console's response is unmeasured

## change [FT_Outline_Copy]

- [FT_Outline_Copy](../../../core/libs/prx/libSceFreeType/src/OutlineOrientation.cpp) (libSceFreeType) - compatibility with the console FreeType build and its errors is unverified. The LP64 guest adapter follows the unsigned counts and copy contract of bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftoutln.c). An unreadable source or target record, unreadable source arrays or unwritable target arrays return `Invalid_Outline` before anything is written; the console's result for them is unmeasured.

## change [FT_Outline_Get_Orientation]

- [FT_Outline_Get_Orientation](../../../core/libs/prx/libSceFreeType/src/OutlineOrientation.cpp) (libSceFreeType) - console ABI/version and large-outline equivalence are unverified. The adapter follows bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftoutln.c) with the existing unsigned guest counts and an LP64 area accumulator, including on Windows. An unreadable outline record, a null or unreadable point array, a null or unreadable contour array with contours, or a contour end at or past `n_points` throws; the console's result for those inputs is unmeasured.
