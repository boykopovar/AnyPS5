# libSceFreeTypeOt

## change The FT_Glyph_Transform, FT_Glyph_StrokeBorder, FT_Glyph_To_Bitmap and FT_New_Library NIDs

- The FT_Glyph_Transform, FT_Glyph_StrokeBorder and FT_Glyph_To_Bitmap NIDs (`AVEEb57HhUw`, `BBRhYfYBDgU`, `c1nbr8oSWlg`) in [Export.cpp](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) are identified, but their provider-specific prototypes and behavior remain unverified. Their named helpers retain the explicit inventory aliases and throw; the separate libSceFreeType implementations do not supply this provider.

## change The 17 `sceFreeTypeOtUnknown_*_nid_no_patch` exports

- The 15 `sceFreeTypeOtUnknown_*_nid_no_patch` exports in [Export.cpp](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) are present in the PS5 3.20 export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Their prototypes and behavior are unverified. Each uses the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.

## add Unknown function info

- [FT_Reference_Library, FT_New_Library](../../../core/libs/prx/libSceFreeType/Export.cpp), [FT_Lookup_Renderer](../../../core/libs/prx/libSceFreeTypeOt/src/Library.cpp) (libSceFreeTypeOt, `qAPXM5DN62Q`, `yrtsLmTfa48`, `4LY5pyZ5IQk`) - console prototypes are unverified; the NIDs are those of the FreeType names. They follow the bundled FreeType 2.15.0 [ftobjs.c](https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/base/ftobjs.c#L4545-L5534), not the provider's 2.4.11, which dereferences a null library in `FT_Reference_Library` and a null `alibrary` in `FT_New_Library`; here both return an error. `FT_New_Library` allocates the wrapper and all library memory on the host, not through the guest `FT_Memory`; the last `FT_Done_FreeType` of such a library throws instead of disposing the record, an earlier one drops a reference. `FT_Lookup_Renderer` returns host renderer and node handles; a node not in the list throws, as does every call with system FreeType.

# libSceFreeTypeOl

## change [`FT_Attach_Stream`, `FT_CMap_New`, and `FT_CMap_Done`]

- [`FT_CMap_New` and `FT_CMap_Done`](../../../core/libs/prx/libSceFreeTypeOl/Export.cpp) (`wLqQQWyOdpc`, `h0n4Fb9j2R8`): direct native calls bypass the existing guest face/charmap layout and lifecycle ownership. These require a dedicated guest bridge before implementation.

## add Signature and layout provenance

- [FT_Attach_Stream](../../../core/libs/prx/libSceFreeType/Export.cpp) (libSceFreeTypeOl, `ZqvTX8u1bEc`) - the console prototype and the LP64 `FT_Open_Args` layout are unverified. It follows the bundled FreeType 2.15.0 [ftobjs.c](https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/base/ftobjs.c#L199-L271), which rejects any mix of `FT_OPEN_MEMORY`, `FT_OPEN_PATHNAME` and `FT_OPEN_STREAM` with `FT_Err_Invalid_Argument`, where the provider's 2.4.11 prefers memory, then pathname, then stream. A guest `FT_Stream` is wrapped as `FT_Open_Face` wraps it and is read and closed through its callbacks. A null pathname throws; a pathname is read into memory as `FT_Attach_File` does, and an empty file returns `FT_Err_Cannot_Open_Stream`. Afterwards only the face's flags, bounding box and metrics are copied back to the guest face.
