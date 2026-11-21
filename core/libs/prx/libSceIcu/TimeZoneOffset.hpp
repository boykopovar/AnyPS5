#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" std::int32_t APS5_VABI uprv_timezone_nid_postfix();
extern "C" std::int32_t APS5_VABI uprv_timezone_at_time_nid_no_patch(std::int64_t time);
