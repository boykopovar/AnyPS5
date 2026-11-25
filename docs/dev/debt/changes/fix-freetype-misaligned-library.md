# libSceFreeType

## add Functional

- [Misaligned library handles](../../../core/libs/prx/libSceFreeType/src/BuildInfo.cpp) (libSceFreeType) - `FT_Library_Version`, `FT_Get_TrueType_Engine_Type`, `FT_Library_SetLcdFilter`, `FT_Library_SetLcdFilterWeights`, `FT_Get_Module`, `FT_Get_Renderer`, `FT_Remove_Module`, `FT_Add_Default_Modules`, `FT_Property_Get`, `FT_Property_Set`, `FT_Outline_New`, `FT_Outline_Done`, `FT_Stroker_New` and `FT_Done_MM_Var` refuse a library handle whose address is not a multiple of `alignof(GuestLibrary)` as they refuse an unreadable one, before reading its host pointer. Every library this module returns is aligned; what the console module does with such a handle has not been measured.
