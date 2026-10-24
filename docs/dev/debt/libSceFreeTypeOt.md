# libSceFreeTypeOt technical debt

### Unknown function info

- [FT_Outline_GetInsideBorder, FT_Outline_GetOutsideBorder](../../../core/libs/prx/libSceFreeTypeOt/CMakeLists.txt) (libSceFreeTypeOl, libSceFreeTypeOt) - console prototypes, enum/layout equivalence, FreeType configuration and module/library versions are unverified. The existing LP64 outline layout and unsigned 16-bit counts follow bundled FreeType 2.13.3; border selection follows [ftstroke.c](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftstroke.c), with the shifted polygon area from [ftoutln.c](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftoutln.c) accumulated in 64 bits. Null positive-count arrays and out-of-range or non-increasing contour endpoints throw when reached; their console behavior is unknown. Tags and flags are not read, and input arrays remain caller-owned.

- The 148 `sceFreeTypeOtUnknown_*_nid_no_patch` exports in [Export.cpp](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) are present in the PS5 3.20 export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Their prototypes and behavior are unverified. Each uses the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.
