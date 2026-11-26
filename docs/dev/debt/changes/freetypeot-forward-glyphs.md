# libSceFreeTypeOt

## add Unknown function info

- [FT_Add_Default_Modules, FT_Done_Glyph, FT_Get_Glyph, FT_Get_Module, FT_Get_Renderer, FT_Get_SubGlyph_Info and FT_Get_TrueType_Engine_Type](../../../core/libs/prx/libSceFreeTypeOt/ForwardExports.cpp) (`pSgOzwkzx4Y`, `RdrrejUMiJ0`, `eMKHL+Cj50k`, `swrIPzOxcmM`, `1wFNGIk3FR0`, `vOhW2Wdz0t4`, `eSFTUAm1wf8`) resolve to the libSceFreeType wrappers like the module's other forwarded exports, so the provider's prototypes are assumed to be FreeType's public ones, unverified for the provider's version. Results, limits and throws are those of [the libSceFreeType wrappers](libSceFreeType.md) over the bundled FreeType 2.15.0, not of the provider's 2.4.11-2.4.12 code, and are not compared with a console. The handles from FT_Get_Module and FT_Get_Renderer are host objects: guests pass them back to this provider's exports unchanged and cannot inspect them.

# libSceFreeType

## change [FT_Get_Module]

- [FT_Get_Module](../../../core/libs/prx/libSceFreeType/src/ModuleManagement.cpp) (libSceFreeType) - console signature is unverified; the adapter forwards to bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftobjs.c). The module handle is a native object owned by the library and is invalid after `FT_Remove_Module` or `FT_Done_FreeType`. libSceFreeType does not export it, but the libSceFreeTypeOt provider forwards it, so guests importing from Ot can obtain the handle. Tests call the wrapper directly, and its console NID/signature remain unverified

## change [FT_Get_Renderer]

- [FT_Get_Renderer](../../../core/libs/prx/libSceFreeType/src/ModuleManagement.cpp) (libSceFreeType) - console signature is unverified; the adapter forwards to bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftobjs.c). The renderer handle is a native object owned by the library and is invalid after `FT_Remove_Module` or `FT_Done_FreeType`. libSceFreeType does not export it, but the libSceFreeTypeOt provider forwards it, so guests importing from Ot can obtain the handle. Tests call the wrapper directly, and its console NID/signature remain unverified

## change [FT_Set_Renderer]

- [FT_Set_Renderer](../../../core/libs/prx/libSceFreeType/src/RendererSelection.cpp) (libSceFreeType) - console signature and the 16-byte LP64 `FT_Parameter` layout are unverified; the adapter follows bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftobjs.c). The renderer handle must come from `FT_Get_Renderer` on the same library; libSceFreeType does not export that getter, but the libSceFreeTypeOt provider forwards it, so guests importing from Ot can obtain it. Tests call the local adapter directly. Parameter data pointers are passed to the native renderer unchanged. A parameter tag above the host `FT_ULong` range throws on Windows, where the native type is 32 bits
