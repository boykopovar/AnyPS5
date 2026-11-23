# libSceFreeType

## add Functional

- [FT_MulDiv, FT_MulFix, FT_DivFix, FT_RoundFix, FT_CeilFix and FT_FloorFix](../../../core/libs/prx/libSceFreeType/src/Arithmetic.cpp) - console signatures, FreeType version, build options and wider-input results are unverified. Names and NIDs follow the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceFreeTypeOt.c). The guest uses the adapter's LP64 ABI and [FreeType 2.13.3 arithmetic](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftcalc.c). The [2.13.3 x86-64 helper](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/include/freetype/internal/ftcalc.h#L180) narrows `FT_MulFix` operands and the result to signed 32 bits. Bundled FreeType 2.15.0 uses wider native `FT_MulFix` arithmetic. FreeType documents arithmetic outside signed 32-bit inputs as potentially undefined.
