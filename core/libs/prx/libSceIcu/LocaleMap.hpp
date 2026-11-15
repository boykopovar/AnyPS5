#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI uprv_convertToPosix_nid_postfix(std::uint32_t, char*, std::int32_t, std::int32_t*);
std::uint32_t APS5_VABI uprv_convertToLCIDPlatform_nid_postfix(const char*, std::int32_t*);
std::uint32_t APS5_VABI uprv_convertToLCID_nid_postfix(const char*, const char*, std::int32_t*);
std::uint32_t APS5_VABI uloc_getLCID_nid_postfix(const char*);
std::int32_t APS5_VABI uloc_getLocaleForLCID_nid_postfix(std::uint32_t, char*, std::int32_t, std::int32_t*);
}
