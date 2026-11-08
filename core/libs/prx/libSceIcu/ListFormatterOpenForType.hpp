#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UListFormatter;
using UListFormatterType = std::int32_t;
using UListFormatterWidth = std::int32_t;

extern "C" {
UListFormatter* APS5_VABI ulistfmt_openForType_nid_postfix(const char* locale, UListFormatterType type,
                                                           UListFormatterWidth width, std::int32_t* status);
}
