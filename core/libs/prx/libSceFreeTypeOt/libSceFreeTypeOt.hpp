#pragma once

#include "prx/libSceFreeTypeOt/include/Stream.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeTypeOt/include/Calc.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"

extern "C" {
int APS5_VABI FT_Trace_Get_Count_nid_no_patch();
const char* APS5_VABI FT_Trace_Get_Name_nid_no_patch(int idx);
}
