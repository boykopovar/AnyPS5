#pragma once

#include "SceTypes.hpp"

extern "C" {

std::int64_t APS5_VABI PKFree_nid_postfix(void** memory);
int APS5_VABI PixelFormatLookup_nid_postfix(void);
int APS5_VABI readIS_nid_postfix(void);
int APS5_VABI writeIS_nid_postfix(void);

}
