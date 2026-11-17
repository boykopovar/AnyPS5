# libSceFreeTypeOt

## change The FT_Glyph_Transform, FT_Glyph_StrokeBorder, FT_Glyph_To_Bitmap and FT_New_Library NIDs

- The FT_Glyph_Transform, FT_Glyph_StrokeBorder and FT_Glyph_To_Bitmap NIDs (`AVEEb57HhUw`, `BBRhYfYBDgU`, `c1nbr8oSWlg`) in [Export.cpp](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) are identified, but their provider-specific prototypes and behavior remain unverified. Their named helpers retain the explicit inventory aliases and throw; the separate libSceFreeType implementations do not supply this provider.

## change The 17 `sceFreeTypeOtUnknown_*_nid_no_patch` exports

- The 15 `sceFreeTypeOtUnknown_*_nid_no_patch` exports in [Export.cpp](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) are present in the PS5 3.20 export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Their prototypes and behavior are unverified. Each uses the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.

## add Unknown function info

- [FT_Reference_Library, FT_New_Library, FT_Lookup_Renderer](../../../core/libs/prx/libSceFreeTypeOt/src/Library.cpp) (libSceFreeTypeOt, `qAPXM5DN62Q`, `yrtsLmTfa48`, `4LY5pyZ5IQk`) - console prototypes are unverified; each NID is that of the FreeType name, and the bodies follow [2.13.3 ftobjs.c](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftobjs.c#L4526-L5522). The library is the libSceFreeType wrapper, not an `FT_LibraryRec`: `FT_New_Library` allocates it and all library memory on the host, never through the guest `FT_Memory`, whose `free` only `FT_Done_FreeType` calls, on the record itself, as ftinit.c does. `FT_Done_FreeType` on a referenced library drops one reference and keeps the host memory manager that upstream also discards. `FT_Lookup_Renderer` returns host renderer and list-node handles; a node not in the library's renderer list throws, and with system FreeType it always throws.

# libSceFreeTypeOl

## change [`FT_Attach_Stream`, `FT_CMap_New`, and `FT_CMap_Done`]

- [`FT_CMap_New` and `FT_CMap_Done`](../../../core/libs/prx/libSceFreeTypeOl/Export.cpp) (`wLqQQWyOdpc`, `h0n4Fb9j2R8`): direct native calls bypass the existing guest face/charmap layout and lifecycle ownership. These require a dedicated guest bridge before implementation.

## add Signature and layout provenance

- [FT_Attach_Stream](../../../core/libs/prx/libSceFreeTypeOl/Export.cpp) (libSceFreeTypeOl, `ZqvTX8u1bEc`) - the console prototype and the LP64 `FT_Open_Args` layout are unverified; it follows [2.13.3 ftobjs.c](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftobjs.c#L2880-L2918). A guest `FT_Stream` (`FT_OPEN_STREAM` with a non-null stream, which upstream reads or closes) and a null pathname throw. A pathname is resolved and read into memory as `FT_Attach_File` does, and an empty file returns `FT_Err_Cannot_Open_Stream` as ftsystem.c does. Afterwards only the face's flags, bounding box and metrics are copied back to the guest face.
