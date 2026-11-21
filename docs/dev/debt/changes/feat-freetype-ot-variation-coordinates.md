# libSceFreeTypeOt

## change The FT_Outline_Get_Orientation, FT_Request_Size
- The FT_Outline_Get_Orientation, FT_Request_Size, FT_Sin, FT_Tan, FT_Vector_Unit, FT_New_Size, FT_Outline_Copy and FT_Outline_Decompose NIDs in [Export.cpp](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) are identified, but their provider-specific prototypes and behavior remain unverified. Their named helpers retain the explicit inventory aliases and throw; the separate libSceFreeType implementations do not supply this provider.

## add Unknown function info
- [Variation coordinate setters](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) use the existing libSceFreeType wrappers for the four matching FreeType NIDs. Guest fixed-point arrays follow the LP64 public ABI and are converted by those wrappers to host-width values. The console provider is unmeasured. For variation design coordinates, the bundled FreeType 2.15.0 defaults omitted axes and clamps values beyond axis bounds. FreeType 2.4.11 can reject those inputs.
