#pragma once

#include "SceTypes.hpp"

extern "C" {

std::int32_t APS5_VABI ucol_countAvailable_nid_postfix();
const char* APS5_VABI ucol_getAvailable_nid_postfix(std::int32_t localeIndex);

}
