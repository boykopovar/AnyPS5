#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UDateTimePatternGenerator;

extern "C" {
std::int32_t APS5_VABI udatpg_getFieldDisplayName_nid_postfix(const UDateTimePatternGenerator* dtpg,
                                                              std::int32_t field,
                                                              std::int32_t width,
                                                              char16_t* fieldName,
                                                              std::int32_t capacity,
                                                              std::int32_t* status);
}
