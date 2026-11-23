# libSceFreeType

## change [FT_Property_Get, FT_Property_Set]

- [FT_Property_Get, FT_Property_Set](../../../core/libs/prx/libSceFreeType/src/ModuleProperties.cpp) - setting `ot-svg` `svg-hooks` to four non-null hooks throws: the hooks would be called with host glyph slots and SVG documents. Known property carriers adapted to guest layouts use checked guest copies; an inaccessible non-null carrier returns `FT_Err_Invalid_Argument` and the copy probe preserves `errno`. Plain typed gets preserve a native property error before attempting guest copy-out. Console signatures, invalid-pointer behavior, property availability and backend/error equivalence are unverified; the adapter follows bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f2/src/base/ftobjs.c#L5317-L5400) under the existing LP64 assumption. Pairs that 2.13.3 does not define are forwarded with the guest pointer, so a system FreeType defining more properties would write host layouts.
