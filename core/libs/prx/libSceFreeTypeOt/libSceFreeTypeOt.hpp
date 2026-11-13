#pragma once
#include "prx/libSceFreeTypeOt/include/MemoryAllocation.hpp"

#include "prx/libSceFreeTypeOt/include/Stream.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeTypeOt/include/Bitmap.hpp"
#include "prx/libSceFreeTypeOt/include/Calc.hpp"
#include "prx/libSceFreeTypeOt/include/GlyphSlot.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libSceFreeTypeOt/include/Outline.hpp"
#include "prx/libSceFreeTypeOt/include/Raccess.hpp"
#include "prx/libSceFreeTypeOt/include/SizeMetrics.hpp"
#include "prx/libSceFreeTypeOt/include/System.hpp"

extern "C" {
int APS5_VABI FT_Glyph_Stroke_nid_postfix();
int APS5_VABI FT_New_GlyphSlot_nid_postfix();
int APS5_VABI FT_Set_Debug_Hook_nid_postfix();
int APS5_VABI FT_Trace_Get_Count_nid_no_patch();
const char* APS5_VABI FT_Trace_Get_Name_nid_no_patch(int idx);
void APS5_VABI FT_Stroker_ExportForward_nid_no_patch(void* stroker, FreeType::GuestOutline* outline);
}
