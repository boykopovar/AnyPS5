# libSceFreeTypeOt

## change The 17 `sceFreeTypeOtUnknown_*_nid_no_patch` exports

- The 16 `sceFreeTypeOtUnknown_*_nid_no_patch` exports in [Export.cpp](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) are present in the PS5 3.20 export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Their prototypes and behavior are unverified. Each uses the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.

## add Unknown function info

- [FT_Reference_Library](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) (libSceFreeTypeOt, NID `qAPXM5DN62Q`, the hash of that name) - console prototype unverified, nothing measured on a console. It calls bundled FreeType 2.15.0 [FT_Reference_Library](https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/base/ftobjs.c#L5486-L5495) on the host library of a libSceFreeType library: the count is incremented and 0 returned, as in the provider's dated [2.4.11](https://github.com/freetype/freetype/blob/c6a66b49e64e9a21c013f23c867d238cf1aa98cb/src/base/ftobjs.c#L4513-L4519); a null library returns `Invalid_Library_Handle`, where 2.4.11 dereferences it. No `FT_Done_Library` is exported, and `FT_Done_FreeType` on a library holding an extra reference deletes the guest library while [FreeType](https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/base/ftinit.c#L238-L260) keeps the host one, so the handle is invalid afterwards
