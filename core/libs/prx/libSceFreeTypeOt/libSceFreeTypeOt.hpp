#pragma once

#include "prx/libSceFreeTypeOt/include/ModuleService.hpp"
#include "prx/libSceFreeTypeOt/include/Stream.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeTypeOt/include/Bitmap.hpp"
#include "prx/libSceFreeTypeOt/include/Calc.hpp"
#include "prx/libSceFreeTypeOt/include/GlyphSlot.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libSceFreeTypeOt/include/Outline.hpp"
#include "prx/libSceFreeTypeOt/include/OutlineTransform.hpp"
#include "prx/libSceFreeTypeOt/include/Polar.hpp"
#include "prx/libSceFreeTypeOt/include/Raccess.hpp"
#include "prx/libSceFreeTypeOt/include/SizeMetrics.hpp"
#include "prx/libSceFreeTypeOt/include/System.hpp"

extern "C" {
int APS5_VABI FT_Glyph_Stroke_nid_postfix();
int APS5_VABI FT_Set_Debug_Hook_nid_postfix();
int APS5_VABI FT_Trace_Get_Count_nid_no_patch();
const char* APS5_VABI FT_Trace_Get_Name_nid_no_patch(int idx);
[[noreturn]] void APS5_VABI ft_validator_error_nid_postfix(void* validator, std::int32_t error);
void APS5_VABI FT_Stroker_ExportForward_nid_no_patch(void* stroker, FreeType::GuestOutline* outline);
void APS5_VABI sceFreeTypeOtGlyphGetCBox_nid_no_patch(FreeType::GuestGlyphRec* glyph, std::uint32_t mode, FreeType::GuestBBox* box);
void APS5_VABI ft_validator_init_nid_postfix(void* validator, const std::uint8_t* base,
                                           const std::uint8_t* limit, std::int32_t level);
}
