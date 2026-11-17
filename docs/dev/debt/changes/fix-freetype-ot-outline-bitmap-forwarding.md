# libSceFreeTypeOt

## change The FT_Glyph_Get_CBox, FT_Set_Renderer,
- The FT_Glyph_Get_CBox, FT_Outline_Get_Orientation, FT_Sin, FT_Tan, FT_Vector_Unit, FT_Outline_Copy, FT_Outline_Decompose, FT_GlyphSlot_Oblique, FT_GlyphSlot_Embolden and FT_Open_Face NIDs in [Export.cpp](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) are identified, but their provider-specific prototypes and behavior remain unverified. Their named helpers retain the explicit inventory aliases and throw; the separate libSceFreeType implementations do not supply this provider.

## add Unknown function info
- [FT_Outline_Get_Bitmap](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) forwards the matching NID to libSceFreeType's guest outline and bitmap adapter. The adapter converts coordinates and renders into the caller's bitmap with the bundled FreeType rasterizer. The console provider remains unmeasured.
- [FT_Set_Renderer](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) forwards to libSceFreeType's renderer selection adapter and uses its guest parameter conversion. Handles must come from the bundled FreeType backend. The console provider remains unmeasured.
